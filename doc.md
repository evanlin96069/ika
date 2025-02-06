# ika Language Documentation

- [Hello World](#hello-world)
- [Comments](#comments)
- [Values](#values)
- [Variables](#variables)
- [Operators](#operators)
- [Control Flow](#control-flow)
   * [if-else](#if-else)
   * [while](#while)
- [Functions](#functions)
- [extern](#extern)
- [Pointers](#pointers)
   * [Function Pointers](#function-pointers)
- [Arrays](#arrays)
- [struct](#struct)
- [sizeof](#sizeof)
- [Defines](#defines)
   * [const](#const)
   * [enum](#enum)
- [#include](#include)

## Hello World
```zig
"Hello world!\n";
```

ika programs execute sequentially from top to bottom like a script.

String literals on their own will be sent to `printf`. You can also pass additional arguments.

```zig
"Hello %s\n", "world";
```

## Comments
```zig
// This is a comment
```

Comments in ika follow the C-style `//` syntax but do not support multi-line comments.

## Values

### Primitive Types

| Type | Description |
| ---- | ----------- |
| `i8` | signed 8-bit integer |
| `u8` | unsigned 8-bit integer |
| `i16` | signed 16-bit integer |
| `u16` | unsigned 16-bit integer |
| `i32` | signed 32-bit integer |
| `u32` | unsigned 32-bit integer |
| `bool` | `true` or `false` |
| `void` | incomplete type |

TODO: Support floating-point numbers and 64-bit integers.

### Primitive Values

| Name | Description |
| ---- | ----------- |
| `true` and `false` | `bool` values |
| `null` | `*void` with value of 0 |

### Integer Literals
```zig
var decimal_int: i32 = 1234;
var hex_int: i32 = 0xff;
var another_hex_int: i32 = 0xFF;
var binary_int: i32 = 0b11110000;
var c: u8 = 'a';
```

Integer literals are type `i32`, or `u32` if they do not fit in `i32`.

### String Literals
```zig
var s: []u8 = "Hello";
```

String literals are pointer to a null-terminated string.

| Escape Sequence | Name |
| --------------- | ---- |
| `\n` | Newline |
| `\r` | Carriage Return |
| `\t` | Tab |
| `\\` | Backslash |
| `\'` | Single Quote |
| `\"` | Double Quote |
| `\0` | Zero Value |

TODO: Add `\xNN`.

## Variables
```zig
var: i32 a = 69;
var: i32 b; // undefined
b = 420;
```
Declare variables using `var`.

Variables declared at the top level are global variables.

Variables declared inside a function or block are local variables and are stored on the stack.

```zig
var a: i32 = 34; // Global variable

if (true) {
    var b: i32 = 35; // Local variable
    "%d\n", a + b;
}

fn func(x: i32) i32 {
    var n: i32 = a; // Local variable
    return x + n;
}
```

## Operators
```zig
var: i32 a = 35;
var: i32 b = 34;
var: i32 c = a + b;

a += b;
b *= 2;
b += 1; 
```
ika supports most C operators except for `++` and `--`.

Compound assignment operators such as `+=` and `-=` are available.

## Control Flow

### if-else
```zig
var a: i32 = 5;
var b: i32 = 6;

if (a == b) {
    "equal!\n";
} else if (a > b) {
    "a is greater!\n";
} else {
    "b is greater!\n";
}
```

### while
```zig
var i: i32 = 0;
while (i < 3) {
    "WAH!\n";
    i += 1;
}
```
You can also use `break` and `continue` as in C.
```zig
var i: i32 = 0;
while (true) {
    if (i == 3) {
        break;
    }

    "WAH!\n";
    i += 1;
}
```

ika does not have `for` loops, but you can specify a statement to execute at the end of a `while` loop.

```zig
var i: i32 = 0;
while (i < 3) : (i += 1) {
    "WAH!\n";
}

var a: i32 = 1;
var b: i32 = 1;
while (a * b < 2000) : (a *= 2, b *= 3) {
    "%d\n", a * b;
}
```

## Functions
```zig
fn add(a: i32, b: i32) i32 {
    return a + b;
}

"%d\n", add(34, 35); // 69
```
Declare functions with `fn`.

You can also forward declare a function.
```zig
fn greeting() void;

greeting();

fn greeting() void {
    "WAH!\n";
}
```

TODO: Allow functions to return `struct` or array types.

## extern
```zig
struct FILE; // Declares an incomplete type
extern var stderr: *FILE;
extern fn fprintf(stream: *FILE, format: []u8, ...) i32;

fprintf(stderr, "Hi!\n");
```
You can mark variables or functions as extern, meaning they are declared externally.

TODO: Support variadic functions (`...`) for user-defined functions. Currently, they are only available for external functions.

## Pointers

- `*T`: single-item pointer to exactly one item.
- `[]T`: many-item pointer to unknown number of items.
    * Supports index syntax: `prt[i]`
    * Supports pointer arithmetic: `ptr + x`, `ptr - x`
    * `T` must have a known size.

Use `&x` to obtain a single-item pointer.

Use `*x` to dereference the pointer.

```zig
var n: i32 = 69;
var p: *i32 = &n;

*p = 420;

"%d\n", n; // 420
```

`*void` can be assigned to or converted to any pointer type.

```zig
extern fn malloc(size: u32) *void;
extern fn free(ptr: *void) void;

var arr: []u8 = malloc(sizeof(u8) * 3);
free(arr);
```

### Function Pointers
```zig
fn add(a: i32, b: i32) i32 {
    return a + b;
}

fn sub(a: i32, b: i32) i32 {
    return a - b;
}

var func: fn (a: i32, b: i32) i32;

func = add;
"%d\n", func(34, 35); // 69
func = sub;
"%d\n", func(34, 35); // -1
```

Declare function pointer just like the function signature.

## Arrays
```zig
const N = 10;
var arr: [N]i32;

var i: u32 = 0;
while (i < N) : (i += 1) {
    arr[i] = i;
}

// Prints 0 1 2 3 ...
i = 0;
while (i < N) : (i += 1) {
    "%d\n", arr[i];
}
```

Unlike C, arrays are passed by value.

```zig
const N = 10;
var arr: [N]i32;

var i: u32 = 0;
while (i < N) : (i += 1) {
    arr[i] = i;
}

fn clear(arr: [N]i32) void {
    var i: u32 = 0;
    while (i < N) : (i += 1) {
        arr[i] = 0;
    }
}

// This won't modify the actual array.
clear(arr);

// Still prints 0 1 2 3 ...
i = 0;
while (i < N) : (i += 1) {
    "%d\n", arr[i];
}
```

Use a pointer to pass an array by reference.

```zig
const N = 10;
var arr: [N]i32;

var i: u32 = 0;
while (i < N) : (i += 1) {
    arr[i] = i;
}

fn clear(arr: []i32) void {
    var i: u32 = 0;
    while (i < N) : (i += 1) {
        arr[i] = 0;
    }
}

// This will modify the actual array.
clear(&arr);

// Prints 0 0 0 0 ...
i = 0;
while (i < N) : (i += 1) {
    "%d\n", arr[i];
}
```

Note that `&arr` is type `*[10]i32`, but single-item pointer to an array can be convert to many-item pointer (`[]i32`).

## struct
```zig
struct Vec {
    x: i32,
    y: i32,
};

var v: Vec;
v.x = 5;
v.y = 6;

"%d %d\n", v.x, v.y; // 5 6
```

Like in C, `.` is used to access `struct` fields. However, unlike C, `.` also automatically dereferences single-item pointers to `struct`.

```zig
struct Vec {
    x: i32,
    y: i32,
};

fn vec_scale(v: *Vec, n: i32) void {
    // We don't use `->` like C.
    v.x *= n;
    v.y *= n;
}

var v: Vec;
v.x = 5;
v.y = 6;

vec_scale(&v, 3);

"%d %d\n", v.x, v.y; // 15 18
```

## sizeof

`sizeof(T)` returns the size of `T` in bytes as a `u32`. The size of T must be known at compile time.

## Defines

### const
```zig
const a = 35;
const b = 35;
const c = a + b - 1; // This is compile-time known

const s = "WAH!"; // A string literal
```
Use `const` to define a compile-time constant or a string literal. It expands to its defined value and cannot be modified.

### enum
```zig
enum {
    A, // A = 0
    B, // B = 1
    C, // C = 2
};
```
`enum` starts at 0 and increments by 1, but you can set values with `=`.

```zig
enum {
    A = 4, // A = 4
    B,     // B = 5
    C      // C = 6
};
```

## Include
```zig
#include "libc.ika"
printf("Hello world!\n");
```
`#include "filename.ika"` works like `#include` in C, inserting the contents of another file. The maximum inclusion depth is 15.
