# The X Programming Language

This repository contains the source code to the compiler written in C for the X systems programming language, which is very much a Rust-flavored C-like language.

## How it works

The X code is first tokenized, then parsed with a classic handcrafted recursive-descent parser.  After this, the variable resolution pass takes place, which hands out unique unambigious names to the variables in the X source code, and, for example, makes sure each variable is actually defined before use.  After the variable resolution pass, we have the typechecking pass, which computes the types of all expressions and ensures we don't mix up inadequate types.  Then, there is a label collection and checking pass, which guarantees that e.g. goto jumps only to a valid label, and so on.  After this, we have IR lowering pass, where the fully resolved, typechecked, and label-checked AST is lowered to a custom intermediate represantion based on the Three Address Code.  Once we have the raw IR, an iterative IR optimization pipeline consisting of constant folding, constant propagation, copy propagation, unreachable code elimination, and dead store elimination, massages the IR until no more optimizations could have been applied.  The next pass takes this IR and translates it to the corresponding assembly-based IR, where a graph coloring-based register allocator with conservative coalescing makes sure the pseudoregisters are mapped to the physical registers and tries to keep the memory traffic low by keeping everything inside the registers.  The assembly generation pass might generate some invalid instructions, like moving from a memory location to another memory location, and this is where the fixup pass comes in, by rewriting the invalid instructions to use scratch registers.  Finally, the optimized assembly code is emitted into a file.  For assembling and linking the code into a final binary executable, we shell out to gcc, as writing an assembler and a linker on top of the compiler is too much work at this point.

### Tokenizer

Instead of buffering an entire token stream, the tokenizer hands out a token to the parser lazily and on an on-demand basis.  It features the zero-copy architecture via fat pointers, thereby avoiding heap allocation for lexeme storage, and maintaining O(1) memory complexity relative to the source size.

The tokenizer uses a direct-mapped dispatch to simulate the transitions of a Deterministic Finite Automaton (DFA). For keywords, the implementation effectively flattens a prefix trie into the logic: when a character identifies a potential keyword (e.g., 'a'), it performs a fixed-length lookahead to match against specific reserved words, falling back to a general identifier submachine if no keyword match is found.

The tokenizer rigorously enforces the Maximal Munch principle, ensuring it consumes the longest valid sequence of characters for operators (e.g., prioritizing `>>=` over `>>` or `>`).

### Parser

The parser is a classic handwritten top-down recursive descent parser.  While fundamentally LL(1), it has a localized `k=2` lookahead mechanism to resolve some syntactic ambiguities, such as differentiating bounded jump labels from variable references.  This might be fixed in the future to require just a single token of lookahead by introducing Rust-style `'labels`.

Operator precedence rules are borrowed from C and are baked directly into the call stack.  To dodge the pitfalls of the infinite loops characteristic of left-recursive grammar rules, the implementation employs EBNF-style iterative consumption, transforming potentially unbounded left-recursion into iterative loops, and coalescing binary operations into left-associative subtrees.

The resulting abstract syntax tree leverages heterogeneous tagged unions to emulate algebraic sum types, allowing a single `struct Expr` or `struct Stmt` pointer to represent any node in the grammar. 

Finally, rather than panicking or relying on global error states, it propagates result structures up the call stack, ensuring fail-fast compilation and precise error bubbling without the overhead of stack-unwinding exception mechanisms.

### Semantic Analysis

The semantic analysis pipeline is comprised of three sequential passes that transform the raw AST into a fully resolved, type-safe representation.  

The first pass acts as the resolver, establishing lexical scoping via an ephemeral linked-list environment.  It performs alpha-conversion on all variable declarations, systematically renaming them to globally unique identifiers to eliminate shadowing and decouple the backend from high-level block scoping.  During the traversal, it also performs early constant propagation by folding enumeration variants into integer literals.  This is bound to change in the future.

After the resolver, a control-flow validation pass enforces intra-procedural integrity.  Operating as a two-pass validator, it binds branches like break and continue to explicit, uniquely generated loop target labels, detects duplicate labels, and then verifies that every goto statement resolves safely to a valid local target, preventing dangling or inter-procedural jumps.

Finally, the typechecker enforces a statically-typed, flow-insensitive (which might change in the future) type system using bottom-up type synthesis.  It checks L-values for mutability constraints and assignment validity, and evaluates a hybrid of structural equivalence for primitives and nominal equivalence for structs.  When operands mismatch but still fall within legal promotion rules, the typechecker dynamically mutates the AST by injecting explicit cast nodes to handle implicit coercions.  In tandem with validation, it calculates memory offsets and injects padding bytes to guarantee that the final data layout strictly adheres to the architectural alignment constraints of the System V ABI.

###  IR

#### The IR lowering pass

The IR lowering pass acts as a bridge between the abstract syntax tree and the flat, machine-like architecture of the backend.  It converts the structured AST nodes into a linear Three-Address Code (TAC) sequence, where operations consist of at most two operands and a destination.

The IR lowering phase employs a state machine (`ExpResult`) to manage the distinction between L-values (memory locations) and R-values (materialized data).  This defers the emission of memory loads.

- `EXPRESULT_PLAIN`: The expression has been fully evaluated into an R-value.  It exists in a virtual register (a temporary IR variable) or as a literal constant.
- `EXPRESULT_DEREF`: The expression yields a pointer to a memory location (e.g., `*ptr`).  The lowering pass prevents premature emission of a LOAD instruction until the parent AST node dictates whether the value is being read from or written to.
- `EXPRESULT_SUBOBJECT`: Handles complex aggregate accesses (e.g., `point.x`).  It defers the final memory access by retaining the base object's identity, the target field's byte offset, and the base type.

The `irfy_expr_and_convert` function acts as the coercion barrier.  When the semantic context requires an R-value (e.g., the right-hand side of an assignment, or an operand in an arithmetic expression), this function intercepts `EXPRESULT_DEREF` and `EXPRESULT_SUBOBJECT` states, forces materialization, and ensure proper L-value-to-R-value conversion.  It automatically emits the necessary `IRInstr_LOAD` or `IRInstr_CPY_FROM_OFFSET` instructions, transforming the memory location into an `EXPRESULT_PLAIN` virtual register.

Structured control flow (e.g., `if`, `while`, `do/while` blocks, and short-circuiting logical operators `&&`, `||`) is dismantled and flattened into unstructured conditional and unconditional jumps (`IRInstr_JZ`, `IRInstr_JMP`) targeting explicitly generated basic block labels (`IRInstr_LBL`).

#### The IR optimization pipeline 

The optimization pipeline operates as an iterative fixed-point combinator.  The main function (`optimize_ir`) repeatedly passes the IR through a suite of transformation passes (constant folding, constant propagation, copy propagation, unreachable code elimination, dead store elimination), until no further mutations occur (a fixed-point is reached), bounded by `MAX_OPTIMIZATION_PASSES` to guarantee termination.

Some of the passes perform linear sweeps over the instruction stream, while others need a full-blown Control Flow Graph (CFG) to identifiy basic block leaders (labels and jump targets) and compute the directed edges (successors) between blocks.  Once this is in place, it is possible to do branch folding, rewrite statically false branches to unconditional JMP instructions and wipe out statically true branches.

### codegen

The next phase is concerned with generating x86_64 asm.

#### The Register Allocator

The X compiler implements an iterative, graph-coloring register allocator with conservative move coalescing.  Operating in a loop bounded by `MAX_REGALLOC_ATTEMPTS`, each pass begins by computing variable liveness and spill costs, building an interference graph to track variables that are alive at the same time, and using Briggs/George heuristics to coalesce redundant move instructions.  The allocator then attempts to assign "colors" (physical registers) to the graph; if it runs out of registers, the uncolored variables are spilled, meaning the function rewrites the assembly to use stack memory for those variables and completely restarts the allocation loop.  Once a successful coloring pass completes with zero spills, the function finalizes the process by mapping variables to their physical "homes", rewriting the final instruction operands, preserving necessary callee-saved registers, stripping out redundant moves, and returning the successfully allocated function.

#### The fixup pass

The primary responsibility of the fixup pass is to enforce the operand constraints of the target machine by rewriting invalid pseudo-instructions into sequences of valid machine instructions.  Unlike the IR where we could have had arbitrary operands in the instructions, this pass ensures that every intermediate assembly instruction maps strictly to a legal encoding in the target ISA (Instruction Legalization).  Besides this, this pass extracts an illegal operand (like a memory address or a large immediate), loading it into a temporary scratch register, and using that register in the target instruction (Operand Hoisting).  

## Features

- basic data types:
  - signed and unsigned integers
  - single- and double precision floating points
  - booleans
  - strings
  - void
  - pointers
- all operators you would expect from this
- control flow:
  - if/else
  - loop {}
  - while
  - do while
  - break/continue
  - goto
- mut keyword for variables
- structures
- unions
- C-like enums
- sizeof
- varargs
- graph-coloring-based register allocator with conservative coalescing
- IR optimizations
  - constant folding
  - constant propagation
  - copy propagation
  - unreachable code elimination
  - dead store elimination
- custom CFG dumper

## Roadmap
- arrays and/or vectors
- methods
- named loops
- for loops
- comments
- switch and/or match
- e.g. f64 literals
- test suite
- macros
- asm
- etc

## Examples

Just imagine you're writing C, but use the Rust syntax:

```rust
extern fn printf(fmt: str, ...) -> i32;

fn main() -> i32 {
  let mut x: i32 = 0;
  while (x < 5) {
    printf("%d\n", x);
    x += 1;
  }
  ret 0;
}
```

## Building

See Makefile.

e.g.

```
make -j16
```

...then run:

```
./compiler spam.x
./spam
```


## License

Licensed under the MIT license, for details see LICENSE.
