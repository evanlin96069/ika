# ika Language Documentation

- [Hello World](#hello-world)
- [Comments](#comments)
- [Variables](#variables)
- [Operators](#operators)
- [Control Flow](#control-flow)
   * [if-else](#if-else)
   * [while](#while)
- [Functions](#functions)
- [Extern](#extern)
- [Define](#define)
   * [def](#def)
   * [enum](#enum)
- [Include](#include)
- [Advanced Features](#advanced-features)
   * [Pointer](#pointer)
   * [String Literals](#string-literals)
   * [Array](#array)
   * [Dot Operator](#dot-operator)
   * [Access Byte](#access-byte)
   * [Function Pointer](#function-pointer)
   * [Structure](#structure)

## Hello World
```zig
"Hello world!\n";
```

ika programs execute sequentially, from top to bottom like a script.

String literals on their own will be sent to `printf`. You can also pass additional arguments.

```zig
"Hello %s\n", "world";
```

## Comments
```zig
// This is a comment
```
Comments in ika are like those in C, but multi-line comments are not supported.

## Variables
```zig
var a = 69;
"%d\n", a;
```
Declare variables using `var`. ika does not have explicit types; all variables are 32-bit integers.

Variables can also be declared without initialization.

```zig
var a;
```

ika does not support floating point numbers.

## Operators
```zig
var a = 35;
var b = 34;
var c = a + b;

// Swap a and b
x ^= y;
y = x ^ y;
x ^= y;

"a=%d, b=%d, c=%d\n", a, b, c; 
```
ika includes most operators from C except for `++` and `--`. It also supports assignment versions like `+=` and `^=`.

## Control Flow

### if-else
```zig
var a = 5;
var b = 6;

if (a == b) {
    "equal!\n";
} else if (a > b) {
    "a\n";
} else {
    "b\n";
}
```

### while
```zig
var i = 0;
while (i < 3) {
    "WAH!\n";
    i += 1;
}
```
You can also use `break` and `continue` as in C.
```zig
var i = 0;
while (1) {
    if (i == 3) {
        break;
    }

    "WAH!\n";
    i += 1;
}
```
ika does not have for loops, but you can add a statement to execute at the end of a while loop.

```zig
var i = 0;
while (i < 3) : (i += 1) {
    "WAH!\n";
}

var a = 1;
var b = 1;
while (a * b < 2000) : (a *= 2, b *= 3) {
    "%d\n", a * b;
}
```

## Functions
```zig
fn add(a, b) {
    return a + b;
}

"%d\n", add(34, 35);
```
Declare functions with `fn`. All functions and arguments are 32-bit integers. The compiler will not check the argument count of a function call; it simply pushes all arguments onto the stack.

If a function does not return a value, it defaults to returning 0.
```zig
fn hello() {
    "Hello!\n";
}

var n = hello(); // This will return 0
"%d\n", n;
```
You can also forward declare a function.
```zig
fn greeting();

greeting();

fn greeting() {
    "WAH!\n";
}
```

## Extern
```zig
extern var stderr;
extern fn fprintf(fp, fmt);

fprintf(stderr, "Hi!\n");
```
You can mark variables or functions as extern, meaning they are declared externally.

## Define

### def
```zig
def a = 35;
def b = 35;
def c = a + b - 1; // This is compile-time known
```
Use `def` to define a compile-time constant. It cannot be modified.

### enum
```zig
enum {
    A, // A = 0
    B, // B = 1
    C, // C = 2
}
"A=%d, B=%d, C=%d\n", A, B, C;
```
Enum is similar to multiple `def` statements. Enum starts at 0 and increments by 1, but you can set values with `=` or change the increment with `:`.

```zig
enum {
    A = 4, // A = 4
    B : 3, // B = 5, increment by 3
    C      // C = 8
}
"A=%d, B=%d, C=%d\n", A, B, C;
```

## Include
```zig
#include "libc.ika"
printf("Hello world!\n");
```
`#include "filename.ika"` functions like `#include` in C, inserting the content of another file. The maximum `#include` depth is 15.

## Advanced Features
ika is a simple language with only integer types, but it supports some advanced features.

### Pointer
```zig
var a = 5;
var p = &a;

*p = 69;

"%d\n", a;
```
All variables are 32-bit integers. Since ika compiles to 32-bit assembly, pointer size is also 32-bit. You can store and manipulate pointers.

### String Literals
```zig
var s = "world";
"Hello, %s\n", s;
```
String literals are pointers and can be assigned to variables. They are `const char*` in C, so do not modify them. Instead, use `malloc` to allocate memory and copy the string.

### Array
```zig
#include "libc.ika"

var arr = malloc(10 * int); // def int = 4;

var i = 0;
while (i < 10) : (i += 1) {
    *(arr + i * int) = i;
}

i = 0;
while (i < 10) : (i += 1) {
    "%d\n", *(arr + i * int);
}

free(arr);
```
You cannot declare arrays on the stack, but you can use `malloc`.

### Dot Operator
```zig
#include "libc.ika"

var arr = malloc(10 * int);

var i = 0;
while (i < 10) : (i += 1) {
    arr.(i * int) = i;
}

i = 0;
while (i < 10) : (i += 1) {
    "%d\n", arr.(i * int);
}

free(arr);
```
`a.b` is syntactic sugar for `*(a + b)`. Instead of `[]`, ika uses `.` for array indexing.

### Access Byte
```zig
#include "libc.ika"

var arr = malloc(27 * char); // def char = 1;

var i = 0;
while (i < 26) : (i += 1) {
    $arr.i = 'a' + i;
}

$arr.26 = '\0';

"%s\n", arr; // Prints a to z

free(arr);
```
In ika, you can only assign or read 32-bit integers at a time. The `$` operator allows single-byte access.

### Function Pointer
```zig
fn greeting(func, name) {
    func(name);
}

fn hi(name) {
    "Hi, %s!\n", name;
}

fn hello(name) {
    "Hello, %s!\n", name;
}

greeting(hi, "Evan");
greeting(hello, "Evan");
```
In ika, you can assign function pointer to variable and call it.

### Structure
```zig
enum {
    vec_x : 4, // 0: enum starts at 0, set next increment by 4
    vec_y : 4, // 4: set next increment by 4
    vec_z : 4, // 8: set next increment by 4
    Vec3,     // 12: size of the "struct"
}

var v = malloc(Vec3); // malloc(12);

v.vec_x = 0;    // *(v + 0) = 0;
v.vec_y = 1;    // *(v + 4) = 1;
v.vec_z = 2;    // *(v + 8) = 2;

free(v);
```
ika does not have types, so there is no `struct`. However, you can simulate a struct using the `enum` trick with the `.` operator. Be cautious as all member names will be in the global namespace.
