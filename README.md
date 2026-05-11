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

### Structs

```
struct Point {
    int x;
    int y;
};

struct Point p;             // declaración
p.x = 10;                   // acceso a miembro
p.y = 20;
print(p.x);                 // 10
print(p.y);                 // 20

struct Point p2;
p2 = p;                     // asignación struct a struct (copia byte a byte)
print(p2.x);                // 10

// Structs anidados
struct Rect {
    struct Point top_left;
    struct Point bot_right;
};

struct Rect r;
r.top_left.x = 1;           // acceso encadenado
r.bot_right.y = 4;
```

Los structs se definen con `struct Nombre { miembros; };`. Los miembros pueden ser de cualquier tipo (`int`, `char`, `short`, `long`) u otros structs ya definidos. El layout en memoria sigue alineación natural del miembro. La asignación entre structs (`a = b`) copia todos los bytes con `rep movsb`.

### Arrays y punteros

```
// Arrays legacy
array vec[10];              // declaración de array (long por defecto)
vec[0] = 42;
print(vec[0]);              // 42

// Arrays tipados (Nivel 8)
char buf[10];               // array de chars (10 bytes)
buf[0] = 72;                // 'H'
int arr[5];                 // array de ints (20 bytes, 5 × 4)
arr[0] = 100;

// Punteros
x = 42;
p = &x;                     // dirección de x
print(*p);                  // 42 (dereferencia)
*p = 99;
print(x);                   // 99

// Punteros tipados (Nivel 8)
int *ip = &x;               // puntero con tipo: load/store usan ancho correcto
int **pp = &p;              // puntero doble
print(**pp);                // 99

// Memoria dinámica (Nivel 8)
extern def malloc(size);
extern def free(ptr);
ptr = malloc(sizeof(long));
*ptr = 42;
free(ptr);
```

### Preprocesador

```
#define MAX 100                 // macro simple
#define SALUDO "hola"           // macro con string
#define MAX(a,b) ((a)>(b)?(a):(b))  // macro función-like
#define SQUARE(x) ((x)*(x))

#include "utils.mat"            // inclusión de archivo (ruta relativa)
```

- `#define` — macros simples (objeto) y función-like con parámetros
- `#include "..."` — inclusión recursiva de archivos, rutas relativas al fuente
- Expansión encadenada (`#define B A` donde `A` es otra macro)
- Protección contra recursión infinita al expandir
- Las macros no se expanden dentro de strings ni char literals

### Optimizaciones (Nivel 11)

El optimizador aplica 4 pases sobre el AST en bucle hasta punto fijo (máx 10 iteraciones):

| Pase | Descripción | Ejemplo |
|------|-------------|---------|
| **Constant folding** | Evalúa expresiones constantes en compilación | `3+4*2` → `11` |
| **Copy propagation** | Sustituye variables por sus valores conocidos | `x=5; y=x;` → `y=5;` |
| **Dead code elimination** | Elimina código inalcanzable | `if(0){...}` → eliminado |
| **Function inlining** | Expande funciones pequeñas inline | `square(5)` → `25` |

```
// Constant folding
print(3 + 4 * 2);               // → print(11)

// Dead code elimination
if (0) { print(999); }          // eliminado
if (1) { print(10); }           // → print(10)
while (0) { print(666); }       // eliminado
return 30;
print(555);                     // eliminado (código tras return)

// Function inlining (funciones ≤3 params, un solo return, sin recursión)
def square(x) { return x * x; }
print(square(5));               // → print(25) (inline + folding)

// Combinado con preprocesador
#define MAX(a,b) ((a)>(b)?(a):(b))
print(MAX(10, 20));             // → print(20)
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

### Forward declarations

```
def is_even(n);                // declaración anticipada (sin cuerpo)

def is_odd(n) {
    if (n == 0) { return 0; }
    return is_even(n - 1);     // llamada a función aún no definida
}

def is_even(n) {               // definición real
    if (n == 0) { return 1; }
    return is_odd(n - 1);
}
```

### Punteros a función

```
def square(x) { return x * x; }

fp = &square;                  // toma la dirección de una función
print(fp(5));                  // 25 — llamada indirecta

def apply(f, x) {
    return f(x);               // parámetro usado como función
}
print(apply(&square, 3));      // 9
```

### Funciones externas (C)

```
extern def printf(str);        // declara función enlazada externamente
                               // (no requiere definición en el fuente)
```

### Punteros tipados

```
int x = 10;
int *p = &x;                 // puntero a int
print(*p);                   // 10 — dereferencia con ancho de tipo (movslq)

int **pp = &p;               // puntero doble
print(**pp);                 // 10

**pp = 99;
print(x);                    // 99
```

Los punteros tienen tipo: `int*`, `char*`, `long*`, etc. El codegen usa la instrucción de carga correcta según el tipo apuntado (`movsbq` para char, `movslq` para int, `movq` para long/punteros).

### Memoria dinámica (malloc/free)

```
extern def malloc(size);     // declarar función externa de C
extern def free(ptr);

p = malloc(sizeof(long));    // reserva 8 bytes
*p = 42;
print(*p);                   // 42
free(p);                     // libera
```

### sizeof

```
print(sizeof(char));         // 1
print(sizeof(short));        // 2
print(sizeof(int));          // 4
print(sizeof(long));         // 8
print(sizeof(long *));       // 8 (puntero, 64-bit)
print(sizeof(int **));       // 8
```

`sizeof(type)` se evalúa en tiempo de compilación como constante numérica.

### Arrays tipados

```
char buf[10];                // array de 10 chars (10 bytes)
buf[0] = 72;                 // 'H'
buf[1] = 105;                // 'i'
print(buf[0]);               // 72

int arr[5];                  // array de 5 ints (20 bytes)
arr[0] = 100;
arr[4] = 200;
print(arr[0]);               // 100
```

Arrays con tipo explícito (`char`, `int`, `short`, `long`) usan el ancho correcto para indexado y load/store.

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
│   ├── compiler.h     # Tipos compartidos: Token, ASTNode, enums, type macros
│   ├── lexer.h / .c   # Análisis léxico (tokenización, comentarios, escapes)
│   ├── parser.h / .c  # Análisis sintáctico (descendente recursivo)
│   ├── preprocessor.h/.c  # Preprocesador (#include, #define), fase previa al lexer
│   ├── optimizer.h / .c # Optimizador AST (folding, copy-prop, dead-code, inline)
│   ├── semantic.h/.c  # Análisis semántico (tabla de símbolos, scopes)
│   ├── codegen.h / .c # Generación de código assembly x86-64
│   ├── struct.h / .c  # Tabla de structs y type_size() público
│   ├── ast.c          # Constructores / destructores / impresión del AST
│   └── main.c         # Driver: orquesta fases y llama a gcc
├── examples/
│   └── test.mat      # Programa de prueba
└── build/            # build/compilador, build/output.s, build/prog
```

## Arquitectura — flujo de compilación

```
fuente.mat
    │
    ▼
┌───────────────┐  fuente     ┌─────────┐   stream de     ┌────────┐   AST en     ┌──────────┐   AST        ┌───────────┐
│ Preprocessor  │───  pp  ──► │  Lexer  │───  tokens  ──► │ Parser │── memoria ──►│ Semantic │── opt ──► │ Optimizer │
└───────────────┘             └─────────┘                 └────────┘              └──────────┘           └───────────┘
    │                           │                        │                          │                        │
    │ lee fuente pp             │ next_token() × N        │ parse_program()        │ tabla símbolos         │ folding
    │ expande macros            │                          │ descendente recursivo  │ chequeo errores        │ copy-prop
    ▼                           ▼                          ▼                        ▼                        │ dead-code
  fuente sin          TOK_IF, TOK_IDENT,         PROGRAM                  AST validado                   │ inline
  directivas          TOK_NUMBER, ...            ├ FUNC(main)                             │               │
  ni macros                                      ├ ASSIGN               ┌─────────┐      │               │
                                                 ├ IF/ELSE      ┌──────►│ Codegen │◄─────┘               │
                                                 ├ SWITCH       │       └─────────┘                      │
                                                 ├ WHILE        │            │                            │
                                                 └ CAST         │       output.s                         │
                                                                │            │                            │
                                                                │       .bss / .text                      │
                                                                │       main:                            │
                                                                │         pushq %rbp                      │
                                                                │         ...                             │
                                                                │         call factorial                  │
                                                                │         ret                             │
                                                                │            │
                                                                │       .bss / .text
                                                                │       main:
                                                                │         pushq %rbp
                                                                │         ...
                                                                │         call factorial
                                                                │         ret
```

Cada fase es independiente y se comunica solo por estructuras de datos:
- **Preprocessor → Lexer**: archivo fuente preprocesado (sin directivas, macros expandidas)
- **Lexer → Parser**: struct `Token` (tipo, lexema, valor, línea, columna)
- **Parser → Semantic**: struct `ASTNode` (árbol enlazado con tipo, operador, hijos)
- **Semantic → Optimizer**: AST validado (4 pases de optimización en bucle)
- **Optimizer → Codegen**: AST optimizado
- **Codegen → gcc**: archivo `output.s` (assembly GAS/AT&T)

## Detalles de implementación

### Preprocesador
- Se ejecuta antes del lexer: lee el fuente original, expande directivas, escribe un archivo temporal preprocesado
- `#include "file"` — inclusión recursiva con resolución de rutas relativas al directorio del archivo actual
- `#define NOMBRE valor` — macros simples con expansión en el texto
- `#define NOMBRE(a,b) cuerpo` — macros función-like con sustitución de parámetros
- Expansión encadenada de macros con protección contra recursión infinita
- Las macros no se expanden dentro de strings ni char literals
- Soporte de continuación de línea con backslash `\`

### Optimizador
- Se ejecuta entre el análisis semántico y el codegen, transformando el AST in-place
- 4 pases en bucle de punto fijo (máx 10 iteraciones) que se habilitan mutuamente
- **Constant folding**: recorre el AST bottom-up; evalúa operaciones binarias, unarias, ternarias y casts con operandos constantes
- **Copy propagation**: dentro de cada bloque básico, sustituye variables por constantes o copias simples; se invalida en saltos y llamadas
- **Dead code elimination**: elimina `if(0)`/`if(1)`/`while(0)` con condiciones constantes, y código inalcanzable tras `return`/`break`/`continue`
- **Function inlining**: expande funciones de un solo `return`, ≤3 parámetros, sin recursión; combinado con folding produce constantes

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
| 6 | `struct` — definición, declaración, acceso a miembros, asignación, structs anidados | ✓ |
| 9 | Forward declarations, punteros a función, `extern` | ✓ |
| 8 | Memoria dinámica — `malloc`/`free`/`sizeof`, punteros tipados, punteros dobles | ✓ |
| 10 | Preprocesador — `#include`, `#define` (simple y función-like), macros encadenadas | ✓ |
| 11 | Optimizaciones — constant folding, copy propagation, dead code, inlining | ✓ |

## Futuras implementaciones

| Nivel | Contenido | Dificultad |
|-------|-----------|------------|
| 7 | `float` y `double` — literales `3.14`, aritmética SSE (xmm), conversión int↔float | Alta |
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
