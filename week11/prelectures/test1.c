#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
	char *a = malloc(1000);
	sprintf(a, "Hello world!");

	if (fork() == 0) {
		printf("Child PID: %d\n", getpid());
		printf("a: %p (%s)\n", a, a);
		sprintf(a, "Goodbye");
		printf("a: %p (%s)\n", a, a);
		sleep(100);
	} else {
		sleep(1);
		printf("Parent PID: %d\n", getpid());
		printf("a: %p (%s)\n", a, a);
		sleep(100);
	}

	free(a);

	return 0;
}
