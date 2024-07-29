#include <stdio.h>

// Recursive function that will eventually overflow the stack
void stackOverflow(int count) {
    // Print the current recursion depth
    printf("Recursion depth: %d\n", count);

    // Call itself to go deeper into recursion
    stackOverflow(count + 1);
}

int main() {
    // Start the recursive calls
    stackOverflow(1);
    return 0;
}
