# minicc - A Simple C Compiler for macOS and Linux

A minimal C compiler written in C that compiles a subset of C to native executables. Supports both macOS (ARM64 Apple Silicon and x86-64 Intel) and Linux (x86-64).

## Features

### Supported Language Features

- **Types**: `int`, `void`
- **Variables**: Local and global variables, arrays
- **Operators**:
  - Arithmetic: `+`, `-`, `*`, `/`, `%`
  - Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
  - Logical: `&&`, `||`, `!`
  - Assignment: `=`, `+=`, `-=`
  - Increment/Decrement: `++`, `--` (prefix only)
  - Address-of: `&`
- **Control Flow**:
  - `if` / `else` statements
  - `while` loops
  - `for` loops
- **Functions**: With parameters and return values
- **Comments**: Single-line (`//`) and multi-line (`/* */`)
- **Standard Library**: `printf` (via libc)

### Limitations

This is an educational compiler with intentional limitations:
- Only `int` type (no `char`, `float`, `double`, pointers, structs)
- No preprocessor (`#include`, `#define`, etc.)
- No strings beyond literals passed to printf
- Arrays are basic (no initialization lists)
- Maximum 6 function parameters
- No switch statements, break, continue

## Building

```bash
# Build the compiler
make

# Or manually:
cc -Wall -O2 -o minicc minicc.c
```

## Usage

```bash
# Compile a C file to executable
./minicc input.c -o output

# Generate assembly only (no linking)
./minicc input.c -S -o output.s

# Default output names (input.c -> input executable, input.s assembly)
./minicc input.c
```

## Examples

Several example programs are included in the `examples/` directory:

```bash
# Build all examples
make examples

# Run all tests
make test
```

### Example Programs

**hello.c** - Hello World
```c
int main() {
    printf("Hello, World!\n");
    return 0;
}
```

**fib.c** - Recursive Fibonacci
```c
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    for (int i = 0; i < 15; i = i + 1) {
        printf("fib(%d) = %d\n", i, fib(i));
    }
    return 0;
}
```

**factorial.c** - Recursive Factorial
```c
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

**primes.c** - Prime Number Finder
```c
int is_prime(int n) {
    if (n < 2) return 0;
    for (int i = 2; i * i <= n; i = i + 1) {
        if (n % i == 0) return 0;
    }
    return 1;
}
```

## How It Works

The compiler follows a traditional compilation pipeline:

1. **Lexer** - Tokenizes the source code into tokens
2. **Parser** - Builds an Abstract Syntax Tree (AST)
3. **Code Generator** - Generates assembly from the AST
4. **Assembler/Linker** - Uses system `cc` to create executable

### Architecture Support

The compiler automatically detects your platform:
- **macOS ARM64** (Apple Silicon M1/M2/M3/M4): Generates ARM64 assembly
- **macOS x86-64** (Intel Mac): Generates x86-64 assembly
- **Linux x86-64**: Generates x86-64 assembly

## Clean Up

```bash
make clean
```

## License

Educational use - feel free to modify and learn from this code!

## Further Reading

If you want to learn more about compiler construction:
- "Crafting Interpreters" by Robert Nystrom
- "Engineering a Compiler" by Cooper & Torczon
- "Modern Compiler Implementation in C" by Andrew Appel
