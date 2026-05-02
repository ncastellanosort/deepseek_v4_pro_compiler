# Compilador Didáctico en C

Compilador sencillo escrito en C con fines educativos. Traduce un lenguaje tipo C a **assembly x86-64** (sintaxis GAS/AT&T) y genera binarios ejecutables mediante `gcc`.

## Ejemplo rápido

```c
print("=== factorial ===");

def factorial(n) {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}

def main() {
    print(factorial(5));    // 120
}
```

```bash
make                          # construye build/compilador
./build/compilador prog.mat   # compila prog.mat → build/prog
./build/prog                  # ejecuta
```

## Características del lenguaje

### Variables y tipos

```
x = 5;                    // implícita (64-bit, → long)
int y = 10;               // explícita 32-bit con inicialización
int z;                    // declarada sin inicializar (→ 0)
char c = 'A';             // char (8-bit, sign-ext al cargar)
short s = 999;            // short (16-bit, sign-ext al cargar)
long L = 123456;          // long (64-bit)
```

### Expresiones

```
// Aritmética
+  -  *  /  %

// Bit a bit
<<  >>  &  |  ^

// Comparación (→ 0 o 1)
==  !=  <  >  <=  >=

// Lógicos
&&  ||  !

// Azúcar sintáctica
x += 5;   x -= 3;   x *= 2;    // compound assignments
x /= 4;   x %= 4;   x <<= 1;
x >>= 1;  x &= 3;   x |= 8;    x ^= 15;

x++;  ++x;  x--;  --x;          // incremento / decremento

max = a > b ? a : b;            // operador ternario
```

### Literales

```
42                             // número entero
'a'   '\n'   '\t'   '\0'      // caracteres (con escape)
'\r'  '\\'   '\''   '\"'      // más escapes
"hola mundo"                   // string
"línea 1\nlínea 2"            // string con escapes
```

### Control de flujo

```
if (x > 0) { print(1); }
if (x) { print(1); } else { print(0); }

while (i < 5) { print(i); i = i + 1; }
do { print(i); i = i + 1; } while (i < 3);

for (i = 0; i < 5; i++)  { print(i); }
for (i = 0; i < 5; ++i)  { print(i); }
for (i = 3; i > 0; i--)  { print(i); }

break;     // dentro de while/for/do-while/switch
continue;  // dentro de while/for/do-while
```

### switch/case/default

```
switch (x) {
    case 1:  print(10); break;
    case 2:  print(20); break;
    default: print(99); break;
}

// fall-through implícito
switch (x) {
    case 1:
    case 2:
        print(12); break;
    default:
        print(0);
}
```

### Casting explícito

```
int a = 65;
char b = (char) a;         // trunca y sign-extiende
print(b);                   // 65 ('A')

short s = (short) 100000;   // trunca a 16-bit → -31072
int i = (int) s;            // sign-extiende de vuelta
long L = (long) i;          // extiende a 64-bit

// Cast en expresiones
print((int) 3.14);          // no-válido aún (float no implementado)
```

### Arrays y punteros

```
array vec[10];           // declaración de array
vec[0] = 42;
print(vec[0]);           // 42

x = 42;
p = &x;                  // dirección de x
print(*p);               // 42 (dereferencia)
*p = 99;
print(x);                // 99
print(p[0]);             // 99 (puntero indexado)
```

### Funciones

```
def suma(a, b) {
    return a + b;
}

def factorial(n) {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}

// hasta 6 parámetros (ABI SysV)
def main() {
    print(suma(3, 4));        // 7
}
```

### Precedencia de operadores (menor a mayor)

```
||  &&  |  ^  &  == !=  < > <= >=  << >>  + -  * / %  -!*& unarios  () [] llamada
```

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

### Ejemplo completo (130 líneas)

```c
/* ── Nivel 2 — Literales y tipos explícitos ─────────────────────── */

print("=== char literals ===");
print('a');              /* 97 */
print('\n');             /* 10 */

print("=== declaracion con init ===");
int x = 100;
char c = 66;
short s = 999;
long L = 123456;
print(x);               /* 100 */
print(c);               /* 66 */
print(s);               /* 999 */
print(L);               /* 123456 */

print("=== declaracion sin init ===");
int y;
print(y);               /* 0 */
y = 50;
print(y);               /* 50 */

print("=== compound assignments ===");
a = 10;
a += 5;  print(a);      /* 15 */
a -= 3;  print(a);      /* 12 */
a *= 2;  print(a);      /* 24 */
a /= 4;  print(a);      /* 6 */
a %= 4;  print(a);      /* 2 */
a <<=1;  print(a);      /* 4 */
a >>=1;  print(a);      /* 2 */
a &= 3;  print(a);      /* 2 */
a |= 8;  print(a);      /* 10 */
a ^=15;  print(a);      /* 5 */

print("=== increment / decrement ===");
z = 0;
z++;  print(z);         /* 1 */
++z;  print(z);         /* 2 */
z--;  print(z);         /* 1 */
--z;  print(z);         /* 0 */

print("=== do-while ===");
i = 0;
do {
    print(i);
    i = i + 1;
} while (i < 3);        /* 0 1 2 */

print("=== ternary ===");
j = 5;
print(j > 3 ? 100 : 200);   /* 100 */

print("=== for con ++ / -- ===");
for (k = 0; k < 3; k++)  { print(k); }      /* 0 1 2 */
for (k = 3; k > 0; k--)  { print(k); }      /* 3 2 1 */

print("=== string escapes ===");
print("tab: \t tab");
print("line1\nline2");

print("=== switch ===");
n = 2;
switch (n) {
    case 1:  print(10); break;
    case 2:  print(20); break;
    default: print(99); break;
}                                           /* 20 */

print("=== casting ===");
int big = 300;
char small = (char) big;
print(small);                               /* 44 (300 truncado a 8-bit) */

print("=== funciones ===");
def suma(a, b) { return a + b; }
def main() { print(suma(10, 20)); }         /* 30 */
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
│   ├── semantic.h/.c # Análisis semántico (tabla de símbolos, scopes)
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
┌─────────┐   stream de     ┌────────┐   AST en     ┌──────────┐
│  Lexer  │───  tokens  ──► │ Parser │── memoria ──►│ Semantic │
└─────────┘                 └────────┘              └──────────┘
    │                           │                        │
    │ next_token() × N          │ parse_program()        │ tabla símbolos
    │                           │ descendente recursivo  │ chequeo errores
    ▼                           ▼                        ▼
  TOK_IF, TOK_IDENT,         PROGRAM                  AST validado ──┐
  TOK_NUMBER, ...            ├ FUNC(main)                             │
                               ├ ASSIGN               ┌─────────┐    │
                               ├ IF/ELSE      ┌──────►│ Codegen │◄───┘
                               ├ SWITCH       │       └─────────┘
                               ├ WHILE        │            │
                               └ CAST         │       output.s
                                              │            │
                                              │       .bss / .text
                                              │       main:
                                              │         pushq %rbp
                                              │         ...
                                              │         call factorial
                                              │         ret
```

Cada fase es independiente y se comunica solo por estructuras de datos:
- **Lexer → Parser**: struct `Token` (tipo, lexema, valor, línea, columna)
- **Parser → Semantic**: struct `ASTNode` (árbol enlazado con tipo, operador, hijos)
- **Semantic → Codegen**: AST validado (misma estructura, sin errores)
- **Codegen → gcc**: archivo `output.s` (assembly GAS/AT&T)

## Detalles de implementación

### Lexer
- Lookahead de 1 carácter (`ch` / `next_ch`)
- Comentarios `//` y `/* */` eliminados en `skip_whitespace_and_comments()`
- Tokens de 2-3 caracteres: `<<` `>>` `<=` `>=` `==` `!=` `&&` `||` `<<=` `>>=`
- Literales de carácter: `'a'`, `'\n'`, `'\0'`, etc.
- Caché de líneas fuente para mensajes de error con subrayado `^`

### Parser
- Descendente recursivo, una función por nivel de precedencia
- `parse_ternary → parse_lor → parse_land → ... → parse_unary → parse_factor`
- `parse_stmt` despacha por keyword: `if`, `while`, `do`, `for`, `return`, `break`, `continue`, `print`, tipos (`int`, `char`, `short`, `long`)
- Soporta dos modos: función (`def`) y retrocompatible (statements sueltos)
- `for` se descompone internamente en init + bloque con step al final
- Operadores compuestos (`+=`, `*=`, etc.) se desugaran a `assign + binary`
- `++`/`--` se desugaran a `x = x ± 1`

### Análisis Semántico
- Tabla de símbolos con scopes anidados: global → función
- Dos pasadas: registro de funciones, luego chequeo de cuerpos
- Chequeos: variable no declarada, redeclaración, args incorrectos, break/continue fuera de loop, return fuera de función
- `print` y funciones built-in pre-declaradas automáticamente

### Generación de código (x86-64 Linux)
- Convención: cada expresión deja su resultado en `%rax`
- Operaciones binarias: `push left; eval right; pop %rcx; op %rcx, %rax`
- Variables globales: `.comm` en BSS con direccionamiento `%rip`
- Variables locales: offsets negativos desde `%rbp`, `subq $N, %rsp` alineado a 16
- Parámetros: `%rdi, %rsi, %rdx, %rcx, %r8, %r9` (ABI SysV, máx 6 args)
- Control de flujo: labels únicas `.L0`, `.L1`, ..., saltos `cmp/je/jmp`
- Break/continue: stack de labels por loop y switch (`ls_start[]`, `ls_cont[]`, `ls_end[]`, `sw_end[]`). Break sale del switch o loop más interno.
- Switch: comparaciones lineales `cmpq/je`, fall-through natural entre cases, label de break compartido
- Tipos reales: load con sign-extensión (`movsbq`/`movswq`/`movslq`), store con truncado (`movb`/`movw`/`movl`), cast explícito (`(type)expr`)
- Escala de arrays por tipo (elementos de 1/2/4/8 bytes)
- Strings escapados correctamente para GAS (caracteres especiales → `\n`, `\t`, etc.)

## Tipos

Cada tipo tiene su ancho real con signo:

| Tipo | Ancho | Load | Store |
|------|-------|------|-------|
| `char` | 8-bit (1 byte) | `movsbq` (sign-ext) | `movb %al` |
| `short` | 16-bit (2 bytes) | `movswq` (sign-ext) | `movw %ax` |
| `int` | 32-bit (4 bytes) | `movslq` (sign-ext) | `movl %eax` |
| `long` | 64-bit (8 bytes) | `movq` | `movq %rax` |

Variables implícitas (sin keyword de tipo, `x = 5`) son `long` por defecto para retrocompatibilidad. Variables locales usan offsets negativos desde `%rbp` con 8-byte de alineación. Los strings son literales de solo lectura en `.rodata`.

## Niveles implementados

| Nivel | Contenido | Estado |
|-------|-----------|--------|
| 1 | Lexer, parser, codegen base — aritmética, control de flujo, funciones | ✓ |
| 2 | Literales char, tipos explícitos, compound assign, `++/--`, do-while, ternario | ✓ |
| 3 | Análisis semántico — tabla de símbolos, scopes, errores | ✓ |
| 4 | `switch/case/default` con fall-through y break | ✓ |
| 5 | Anchura real de tipos: `char`=8bit, `short`=16bit, `int`=32bit, `long`=64bit, casting | ✓ |

## Futuras implementaciones

| Nivel | Contenido | Dificultad |
|-------|-----------|------------|
| 6 | `struct` — declaración, acceso a miembros, structs anidados | Alta |
| 7 | `float` y `double` — literales `3.14`, aritmética SSE (xmm), conversión int↔float | Alta |
| 8 | Memoria dinámica — `malloc`/`free`/`sizeof`, punteros dobles, strings asignables | Alta |
| 9 | Forward declarations, punteros a función, `extern` funciones de C | Media |
| 10 | Preprocesador — `#include`, `#define` (simples y con parámetros) | Media |
| 11 | Optimizaciones — constant folding, copy propagation, dead code, inlining | Media |
| 12 | IR intermedia — three-address code, SSA | Muy alta |
| 13 | Backends adicionales — ARM64, WASM, RISC-V | Muy alta |

## Requisitos

| Dependencia | Versión |
|---|---|
| gcc | cualquiera con soporte x86-64 |
| make | cualquiera |
| Linux x86-64 | — |

## Créditos

Vibe codeado con **DeepSeek** — código generado en sesiones interactivas de prompting iterativo.

## Licencia

Este proyecto es puramente educativo. Libre uso y modificación.
