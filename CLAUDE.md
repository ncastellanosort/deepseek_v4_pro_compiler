# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
make                    # build → build/compilador
./build/compilador prog.mat              # compile → build/output.s → build/prog
./build/compilador prog.mat --dump-ast   # print AST only, skip codegen
./build/prog            # run the compiled binary
make test               # build + compile examples/test.mat
make clean              # remove build/
```

The compiler generates x86-64 GAS/AT&T assembly in `build/output.s`, then shells out to `gcc -no-pie` to produce the final `build/prog` binary.

## Architecture

Five-phase pipeline, each phase independent and communicating only through data structures:

```
source.mat  →  Lexer  →  Token stream  →  Parser  →  AST  →  Semantic  →  validated AST  →  Codegen  →  output.s  →  gcc  →  prog
```

- **Lexer** (`lexer.c`): single-char lookahead (`ch`/`next_ch`), strips `//` and `/* */` comments. Caches source lines for error messages with `^` underlining. 2-3 char multi-char tokens (`<<`, `<=`, `&&`, `<<=`, etc.).
- **Parser** (`parser.c`): recursive descent, one function per precedence level (`parse_ternary → parse_lor → ... → parse_unary → parse_factor`). `parse_stmt` dispatches by keyword. Desugars compound assigns (`x += 5` → `x = x + 5`), inc/dec (`x++` → `x = x + 1`), and `for` (→ init + while-like body with step appended).
- **Semantic** (`semantic.c`): nested scopes (linked list with parent pointer). Two passes in func mode: first registers all functions, then checks bodies. Validates: undeclared vars, redeclarations, arg count mismatch, break/continue outside loops, return outside functions. `print` is auto-declared as a function with 1 param. Accumulates errors and exits if any found.
- **Codegen** (`codegen.c`): generates x86-64 GAS/AT&T assembly. Every expression leaves its result in `%rax`. Binary ops: push left, eval right, pop into `%rcx`, op `%rcx, %rax`. Global vars in `.comm` addressed via `%rip`. Locals as negative `%rbp` offsets, stack aligned to 16. SysV ABI with up to 6 args in `%rdi,%rsi,%rdx,%rcx,%r8,%r9`. Sequential labels `.L0`, `.L1`, ...
- **gcc** (`main.c`): the final step invokes `gcc -no-pie build/output.s -o build/prog`.

## AST structure

The AST is a unified `ASTNode` struct (`compiler.h:60-68`) that uses a union of fields rather than separate structs per node type:

- `left` / `right` / `next` — child pointers. `next` is used for statement lists (PROGRAM, BLOCK) and extra children (TERNARY `next`=else branch, DECL `left`=init expr).
- `op` (char) — holds the operator for BINARY and UNARY nodes. Special operators use uppercase constants: `OP_LSHIFT='L'`, `OP_RSHIFT='R'`, `OP_EQ='E'`, `OP_NE='N'`, `OP_LE='e'`, `OP_GE='g'`, `OP_LAND='A'`, `OP_LOR='O'`, `OP_NOT='!'`, `OP_DEREF='d'`, `OP_ADDR='a'`.
- `num_value` — polymorphic: holds integer value for NUMBER, type tag (TYPE_INT=0, TYPE_CHAR=1, TYPE_SHORT=2, TYPE_LONG=3) for DECL/CAST, array size for ARRAY_DECL, case value for CASE.
- `var_name[MAX_LEXEME]` — holds identifier for VARIABLE, function name for FUNC/CALL, string content for STRING, array name for INDEX/ARRAY_DECL, variable name for DECL.

IF/ELSE chains use `next`: an IF node's `next` points to the paired ELSE node. The checker and codegen both handle this pattern explicitly (skip over the ELSE when advancing through a statement list after processing an IF-with-ELSE).

## Key conventions

- **Two language modes**: "func mode" (source contains `def` functions, everything must be inside functions) and "retrocompatible mode" (loose top-level statements wrapped in an implicit `main`). Detected by checking if the first statement is `AST_FUNC`.
- **Variables**: implicit (no type keyword, `x = 5`) default to `long`. Arrays declared with `array name[size]` also default to `long`.
- **Error handling**: `lexer_error` prints and exits immediately. Semantic errors accumulate and call `exit(1)` after reporting all errors with a count. Codegen calls `exit(1)` on internal errors.
- **C style**: C11 (`-std=c11`), compiled with `-Wall -Wextra -g`. Code is deliberately compact with one-liner functions and minimal/no comments.
- **String escaping**: the codegen emits strings with GAS-compatible escapes (`\n`, `\t`, `\r`, `\\`, `\"`).
- **Break/continue stacks**: `loop_depth`/`sw_depth` track nesting. `ls_start[]`, `ls_cont[]`, `ls_end[]` hold loop labels; `sw_end[]` holds switch break targets. `break` jumps to the innermost loop-end or switch-end; in a switch, break takes priority over loop break.

## Tests

Tests are `.mat` source files in `tests/`. Naming convention:
- `sem_err_*.mat` — semantic error test cases (should fail compilation with a specific error)
- `sem_pass_*.mat` — semantic analysis pass cases (should compile and run correctly)
- `switch_*.mat` — switch/case test cases
- `type_*.mat` — type system tests (casting, widths, arrays)

There is no automated test runner; tests are validated manually by compiling and running each file.
