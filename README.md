# 🦑 ika

A simple programming language that compiles into x86 assembly.

## Build
```sh
make
```
Currently, Windows build is not supported yet.

## Usage
```
Usage: ikac [options] file
Options:
  -E               Preprocess only; do not compile, assemble or link.
  -S               Compile only; do not assemble or link.
  -o <file>        Place the output into <file>.
  -?               Display this information.
```

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

## Documentation

See the [ika Language Documentation](doc.md)
