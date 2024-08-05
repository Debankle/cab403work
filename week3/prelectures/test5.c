#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int main() {
	if (fork()) {
		printf("I'm the parent\n");
	} else {
		execl("/usr/bin/ls",NULL);
	}


	/*
	fork();
	fork();
	printf("Hello world!\n");
	*/


	/*
	 * execl("/usr/bin/ls",".",NULL);
	 * printf("This code will never run\n");
	 */
	return 0;
}
