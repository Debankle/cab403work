#include <stdio.h>
#include <stdlib.h>
int main() {

	printf("Input a string - ");
	char userInput[100], reversed[100];
	scanf("%s", userInput);
	int begin, end, count = 0;

	while (userInput[count] != '\0') count++;

	end = count - 1;
	for (begin = 0; begin < count; begin++) {
		reversed[begin] = userInput[end];
		end--;
	}

	reversed[begin] = '\0';

	printf("%s\n", reversed);

	return 0;
}
