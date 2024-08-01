#include <stdio.h>
#include <stdlib.h>



int main(int argc, char **argv) {
	char *cityPtr[4] = {"Toowong","Chermside","Taringa","Indooroopilly"};

	for (size_t i = 0; i < 4; i++) {
		char *ptr = cityPtr[i];
		while (*ptr != '\0') {
			printf("%c",*ptr);
			ptr++;
		}
		printf("\n");
	}
	return 0;
}
