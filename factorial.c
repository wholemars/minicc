// Factorial calculation
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    printf("Factorials:\n");
    for (int i = 0; i <= 10; i = i + 1) {
        printf("%d! = %d\n", i, factorial(i));
    }
    return 0;
}
