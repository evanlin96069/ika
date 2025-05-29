# 🦑 ika

A simple programming language that compiles into x86 assembly.

## Build

### Prerequisites

* CMake (>= 3.15)
* GCC

---

### Linux

```bash
# From the project root:
mkdir -p build/debug && cd build/debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
cmake --build .

# For release build:
cd ..\..
mkdir -p build/release && cd build/release
cmake -DCMAKE_BUILD_TYPE=Release ../..
cmake --build .
```
The compiler executable (`ikac`) will be placed in `build/<cfg>/bin/`.

### Windows

The project currently supports Windows builds using the MinGW toolchain.
You need a 32-bit MinGW gcc.

```cmd
REM From the project root:
mkdir build\debug
cd build\debug
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ../..
cmake --build .

REM For release build:
cd ..\..
mkdir build\release
cd build\release
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ../..
cmake --build .
```

The compiler executable (`ikac.exe`) will be placed in `build/<cfg>/bin/`.

---

## Testing

### Run all tests

```bash
# From any build directory:
ctest --output-on-failure
```

### Filter by group label

```bash
# Run only the 'basic' tests:
ctest -L basic

# Run 'function' tests in Release mode:
ctest -C Release -L function
```

---

## Usage
```
Usage: ikac [options] file
Options:
  -E               Preprocess only; do not compile, assemble or link.
  -S               Compile only; do not assemble or link.
  -o <file>        Place the output into <file>.
  -?               Display this information.
```

---

## Examples

### Hello world
```zig
"Hello world!\n";
```

### Fibonacci
```zig
// Fibonacci sequence using recursion

fn fib(n: i32) i32 {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() void {
    var i: i32 = 0;
    while (i < 10) : (i += 1) {
        "%d\n", fib(i);
    }
}

main();
```

See more examples in the `examples` and `tests` folder.

---

## Documentation

See the [ika Language Documentation](doc.md)
