#include <stdio.h>

int sum(int *, int);

int main() {
	int array[] = {8,70,10,65,91,81,27,17,14,30};
	int result = sum(array, sizeof(array)/sizeof(int));
	printf("The sum of the array is %d\n", result);
	return 0;
}
