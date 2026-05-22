# The X Programming Language

This repository contains the source code to the compiler written in C for the X systems programming language, which is very much a Rust-flavored C-like language.

## How it works

The X code is first tokenized, then parsed with a recursive-descent parser.  After this, there is a variable resolution pass, which gives the variables their unique unambigious names, and makes sure each variable is actually defined before use.  After the variable resolution pass, we have the typechecking pass, which computes the types of all expressions and ensures we don't mix up inadequate types.  Then, we have a label collection and checking pass, which guarantees that e.g. goto jumps only to a valid label, and so on.  After this, we have IR lowering pass, where the fully resolved, typechecked, and label-checked AST is lowered to a custom intermediate represantion based on the Three Address Code.  The next pass takes this IR and translates it to the corresponding assembly-based IR, which is then finally emitted into a file.  For assembling and linking the code into a final binary executable, we shell out to gcc, as writing an assembler and a linker on top of the compiler is too much work at this point.

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
  - while
  - break/continue
  - goto
- mut keyword for variables
- structures
- unions
- C-like enums

## Roadmap
- sizeof
- named loops
- for loops
- comments
- graph coloring-based register allocator
- IR optimizations
  - constant folding
  - dead store elimination
  - copy propagation
  - unreachable code elimination
- e.g. f64 literals
- etc

## License

Licensed under the MIT license, for details see LICENSE.
