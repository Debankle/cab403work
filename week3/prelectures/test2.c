#include <stdio.h>
#include <stdlib.h>

int main() {
	void *a = malloc(100);
	void *b = malloc(100);
	printf("a: %p\n",a);
	printf("b: %p\n",b);
	free(a);
	free(b);
	return 0;
}
