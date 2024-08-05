#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
	printf("My process id is %d\n", getpid());
	return 0;
}
