#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared.h"


int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stdout, "Usage: %s {name} {lowest floor} {highest floor} {delay}\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    return 0;
}