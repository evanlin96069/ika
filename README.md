# ika

A simple programming language that compiles into 32-bit x86 assembly.

## Build
```sh
make
```
Currently, Windows build is not supported yet.

## Examples

### Hello world
```zig
"Hello world!\n"
```

### Fibonacci
```zig
// Fibonacci sequence using recursion

fn fib(n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    var i = 0;
    while (i < 10) : (i += 1) {
        "%d\n", fib(i);
    }
}

main();
```

See more examples in the `example` folder.

## Documentation

See the [ika Language Documentation](doc.md)
