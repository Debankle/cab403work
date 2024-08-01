#include <stdio.h>
#include <stdlib.h>

void * f() {
	void *p = malloc(100);
	p = malloc(200);
	return p;
}

int main() {
	void *q = f();
	return 0;
}
