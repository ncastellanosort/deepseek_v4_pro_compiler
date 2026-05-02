# Compilador Didáctico en C

Compilador sencillo escrito en C con fines educativos. Traduce un lenguaje tipo C a **assembly x86-64** (sintaxis GAS/AT&T) y genera binarios ejecutables mediante `gcc`.

## Características del lenguaje

```
// Variables
x = 5;
y = x + 3 * (2 - 1);

// Aritméticos
%   <<  >>  &  |  ^

// Comparación  (→ 0 o 1)
==  !=  <  >  <=  >=

// Lógicos
&&  ||  !

// Strings
print("hola mundo");

// Control de flujo
if (x > 0) { print(1); }
if (x) { print(1); } else { print(0); }
while (i < 5) { print(i); i = i + 1; }
for (i = 0; i < 5; i = i + 1) { print(i); }
break;    // dentro de while/for
continue; // dentro de while/for

// Arrays
array vec[10];
vec[0] = 42;
print(vec[0]);

// Punteros
x = 42;
p = &x;
print(*p);      // 42
*p = 99;
print(x);       // 99
print(p[0]);    // 99  (puntero indexado)

// Funciones
def factorial(n) {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}
def main() {
    print(factorial(5));  // 120
}
```

### Precedencia de operadores (menor a mayor)

```
||  &&  |  ^  &  == !=  < > <= >=  << >>  + -  * / %  -!*& unarios  () [] llamada
```

### Tipos

Todo es entero con signo de 64 bits (`long`). Las variables son globales en modo simple o locales a cada función en modo `def`. Los strings son literales de solo lectura en `.rodata`.

## Compilar y usar

```bash
make                          # construye build/compilador
./build/compilador prog.mat   # compila prog.mat → build/prog
./build/prog                  # ejecuta
```

### Flags

```bash
./build/compilador prog.mat --dump-ast    # muestra el AST sin compilar
```

## Estructura del proyecto

```
compiler/
├── Makefile
├── README.md
├── src/
│   ├── compiler.h    # Tipos compartidos: Token, ASTNode, enums
│   ├── lexer.h / .c  # Análisis léxico (tokenización)
│   ├── parser.h / .c # Análisis sintáctico (descendente recursivo)
│   ├── codegen.h / .c# Generación de código assembly x86-64
│   ├── ast.c         # Constructores / destructores / impresión del AST
│   └── main.c        # Driver: orquesta fases y llama a gcc
├── examples/
│   └── test.mat      # Programa de prueba
└── build/            # build/compilador, build/output.s, build/prog
```

## Arquitectura — flujo de compilación

```
fuente.mat
    │
    ▼
┌─────────┐   stream de     ┌────────┐   AST en     ┌─────────┐
│  Lexer  │───  tokens  ──► │ Parser │── memoria ──►│ Codegen │
└─────────┘                 └────────┘              └─────────┘
    │                           │                        │
    │ next_token() × N          │ parse_program()        │ output.s
    │                           │ descendente recursivo  │
    ▼                           ▼                        ▼
  TOK_IF, TOK_IDENT,         PROGRAM                  .bss
  TOK_NUMBER, ...            ├ FUNC(main)             .text
                               ├ ASSIGN               main:
                               ├ IF/ELSE                pushq %rbp
                               ├ WHILE                  ...
                               └ BINARY(+)              call factorial
                                                         ret
```

Cada fase es independiente y se comunica solo por estructuras de datos:
- **Lexer → Parser**: struct `Token` (tipo, lexema, valor, línea, columna)
- **Parser → Codegen**: struct `ASTNode` (árbol enlazado con tipo, operador, hijos)
- **Codegen → gcc**: archivo `output.s` (assembly GAS/AT&T)

## Detalles de implementación

### Lexer
- Lookahead de 1 carácter (`ch` / `next_ch`)
- Comentarios `//` y `/* */` eliminados en `skip_whitespace_and_comments()`
- Tokens de 2 caracteres: `<<` `>>` `<=` `>=` `==` `!=` `&&` `||`
- Caché de líneas fuente para mensajes de error con subrayado `^`

### Parser
- Descendente recursivo, una función por nivel de precedencia
- `parse_expr → parse_land → ... → parse_unary → parse_factor`
- `parse_stmt` despacha por keyword: `if`, `while`, `for`, `return`, `break`, `continue`, `print`
- Soporta dos modos: función (`def`) y retrocompatible (statements sueltos)
- `for` se descompone internamente en init + bloque con step al final

### Generación de código (x86-64 Linux)
- Convención: cada expresión deja su resultado en `%rax`
- operaciones binarias: `push left; eval right; pop %rcx; op %rcx, %rax`
- Variables globales: `.comm` en BSS con direccionamiento `%rip`
- Variables locales: offsets negativos desde `%rbp`, `subq $N, %rsp` alineado a 16
- Parámetros: `%rdi, %rsi, %rdx, %rcx, %r8, %r9` (ABI SysV, máx 6 args)
- Control de flujo: labels únicas `.L0`, `.L1`, ..., saltos `cmp/je/jmp`
- Break/continue: stack de labels por loop (`ls_start[]`, `ls_cont[]`, `ls_end[]`)
- Strings: `.rodata` con labels `.LS0`, `.LS1`, ...

## Requisitos

| Dependencia | Versión |
|---|---|
| gcc | cualquiera con soporte x86-64 |
| make | cualquiera |
| Linux x86-64 | — |

## Créditos

Vibe codeado con **DeepSeek V4 Pro** — 100% del código generado en sesiones interactivas de prompting iterativo.

## Licencia

Este proyecto es puramente educativo. Libre uso y modificación.
