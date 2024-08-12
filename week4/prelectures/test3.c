#include <stdlib.h>
#include <stdio.h>

int comparison(const void *a, const void *b) {
	return *((const int *)a) - *((const int *)b);
}

int main() {
	int a[] = {4,5,6,1,2,3,7,8,9};

	qsort(a,sizeof(a)/sizeof(int), sizeof(int), comparison);

	for (int i = 0; i < sizeof(a)/sizeof(int); i++) {
		printf("%d ", a[i]);
	}

	return 0;
}
