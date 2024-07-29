#include <stdio.h>

void *f(void) {
    void *p = malloc(100);
    p = malloc(200);
    return p;
}

int main(int argc, char **argv) {
}