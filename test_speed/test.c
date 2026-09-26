#include <stdio.h>
#include <stdlib.h>

int multiply(int a, int b) {
    return a * b;
}

int compute(int x, int y) {
    return multiply(x, y) + 2;
}

int main(void) {
    int factor = 5;
    int count = 3;
    int answer = compute(factor, 8);

    // Check if compute(5, 8) = 42
    if (answer == 42) {
        printf("[PASS] Function calls and math returned 42!\n");
    } else {
        printf("[FAIL] Unexpected math result!\n");
    }

    // Loop using a variable (count = 3)
    printf("[*] Starting loop over variable...\n");
    for (int A_Index = 0; A_Index < count; A_Index++) {
        if (A_Index == 0) {
            printf(" -> Iteration 0 (A_Index = 0)\n");
        }
        if (A_Index == 1) {
            printf(" -> Iteration 1 (A_Index = 1)\n");
        }
        if (A_Index == 2) {
            printf(" -> Iteration 2 (A_Index = 2)\n");
        }
    }

    printf("[+] All tests completed successfully!\n");
    exit(0);
}
