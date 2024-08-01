#include <stdio.h>
#include <stdlib.h>

void print_string(char *input) {
	printf("String output: %s\n", input);
}

int main(int argc, char **argv) {
	char *buffer = (char*)malloc(100*sizeof(char));
	if (buffer == NULL) {
		perror("Could not allocate memory\n");
		return -1;
	}
	printf("Enter string: ");
	scanf("%s",buffer);
	print_string(buffer);
	return 0;
}
