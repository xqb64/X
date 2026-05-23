# The X Programming Language

This repository contains the source code to the compiler written in C for the X systems programming language, which is very much a Rust-flavored C-like language.

## How it works

The X code is first tokenized, then parsed with a classic handcrafted recursive-descent parser.  After this, the variable resolution pass takes place, which hands out unique unambigious names to the variables in the X source code, and, for example, makes sure each variable is actually defined before use.  After the variable resolution pass, we have the typechecking pass, which computes the types of all expressions and ensures we don't mix up inadequate types.  Then, there is a label collection and checking pass, which guarantees that e.g. goto jumps only to a valid label, and so on.  After this, we have IR lowering pass, where the fully resolved, typechecked, and label-checked AST is lowered to a custom intermediate represantion based on the Three Address Code.  Once we have the raw IR, an iterative IR optimization pipeline consisting of constant folding, constant propagation, copy propagation, unreachable code elimination, and dead store elimination, massages the IR until no more optimizations could have been applied.  The next pass takes this IR and translates it to the corresponding assembly-based IR, where a graph coloring-based register allocator with conservative coalescing makes sure the pseudoregisters are mapped to the physical registers and tries to keep the memory traffic low by keeping everything inside the registers.  The assembly generation pass might generate some invalid instructions, like moving from a memory location to another memory location, and this is where the fixup pass comes in, by rewriting the invalid instructions to use scratch registers.  Finally, the optimized assembly code is emitted into a file.  For assembling and linking the code into a final binary executable, we shell out to gcc, as writing an assembler and a linker on top of the compiler is too much work at this point.

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
  - break/continue
  - goto
- mut keyword for variables
- structures
- unions
- C-like enums
- sizeof
- graph-coloring-based register allocator with conservative coalescing
- IR optimizations
  - constant folding
  - constant propagation
  - copy propagation
  - unreachable code elimination
  - dead store elimination
- custom CFG dumper

## Roadmap
- named loops
- for loops
- do while loops
- comments
- switch and/or match
- e.g. f64 literals
- etc

## License

Licensed under the MIT license, for details see LICENSE.
