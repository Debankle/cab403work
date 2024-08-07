#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
int main() {
	printf("Process %d is running\n",getpid());
	while (true) {}
	return 0;
}	
