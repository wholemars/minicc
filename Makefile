# Makefile for minicc - A Simple C Compiler

CC = cc
CFLAGS = -Wall -O2

.PHONY: all clean examples test

all: minicc

minicc: minicc.c
	$(CC) $(CFLAGS) -o minicc minicc.c

# Build all example programs using minicc
examples: minicc
	@echo "Building example programs..."
	./minicc examples/hello.c -o hello
	./minicc examples/fib.c -o fib
	./minicc examples/factorial.c -o factorial
	./minicc examples/primes.c -o primes
	./minicc examples/test_all.c -o test_all

# Run tests
test: examples
	@echo "\n=== Running hello ==="
	./hello
	@echo "\n=== Running fib ==="
	./fib
	@echo "\n=== Running factorial ==="
	./factorial
	@echo "\n=== Running primes ==="
	./primes
	@echo "\n=== Running test_all ==="
	./test_all

clean:
	rm -f minicc hello fib factorial primes test_all
	rm -f examples/*.s *.s
