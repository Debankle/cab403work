#include <stdio.h>

int main(int argc, char **argv) {
    printf("\nThe number of command line arguments is %d\n", argc);

    for (int i = 0; i < argc; i++) {
        printf("Command line argument %d is %s\n", i + 1, argv[i]);
    }

    return 0;
}
