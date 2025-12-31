// Test various language features

int global_var = 42;

int add(int a, int b) {
    return a + b;
}

int max(int a, int b) {
    if (a > b) {
        return a;
    }
    return b;
}

int sum_to_n(int n) {
    int sum = 0;
    int i = 1;
    while (i <= n) {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}

int main() {
    // Test arithmetic
    int a = 10;
    int b = 3;
    printf("Arithmetic tests:\n");
    printf("%d + %d = %d\n", a, b, a + b);
    printf("%d - %d = %d\n", a, b, a - b);
    printf("%d * %d = %d\n", a, b, a * b);
    printf("%d / %d = %d\n", a, b, a / b);
    printf("%d %% %d = %d\n", a, b, a % b);
    
    // Test comparisons
    printf("\nComparison tests:\n");
    printf("%d == %d: %d\n", a, b, a == b);
    printf("%d != %d: %d\n", a, b, a != b);
    printf("%d < %d: %d\n", a, b, a < b);
    printf("%d > %d: %d\n", a, b, a > b);
    
    // Test function calls
    printf("\nFunction tests:\n");
    printf("add(%d, %d) = %d\n", a, b, add(a, b));
    printf("max(%d, %d) = %d\n", a, b, max(a, b));
    printf("sum_to_n(10) = %d\n", sum_to_n(10));
    
    // Test global variable
    printf("\nGlobal variable: %d\n", global_var);
    
    // Test for loop
    printf("\nFor loop (squares):\n");
    for (int i = 1; i <= 5; i = i + 1) {
        printf("%d^2 = %d\n", i, i * i);
    }
    
    // Test logical operators
    printf("\nLogical operators:\n");
    int x = 1;
    int y = 0;
    printf("%d && %d = %d\n", x, y, x && y);
    printf("%d || %d = %d\n", x, y, x || y);
    printf("!%d = %d\n", x, !x);
    
    printf("\nAll tests completed!\n");
    return 0;
}
