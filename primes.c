// Prime number finder using sieve-like approach
int is_prime(int n) {
    if (n < 2) return 0;
    for (int i = 2; i * i <= n; i = i + 1) {
        if (n % i == 0) return 0;
    }
    return 1;
}

int main() {
    printf("Prime numbers from 1 to 50:\n");
    int count = 0;
    for (int i = 1; i <= 50; i = i + 1) {
        if (is_prime(i)) {
            printf("%d ", i);
            count = count + 1;
        }
    }
    printf("\nFound %d primes\n", count);
    return 0;
}
