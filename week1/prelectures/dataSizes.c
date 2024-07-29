#include <stdio.h>

int main() {
	printf("Here are the sizes of some fundamental types:\n\n");
	printf("       char: %ld bytes\n", sizeof(char));
	printf("      short: %ld bytes\n", sizeof(short));
	printf("        int: %ld bytes\n", sizeof(int));
	printf("       long: %ld bytes\n", sizeof(long));
	printf("   unsigned: %ld bytes\n", sizeof(unsigned));
	printf("      float: %ld bytes\n", sizeof(float));
	printf("     double: %ld bytes\n", sizeof(double));
	printf("long double: %ld bytes\n", sizeof(long double));
	return 0;
}
