#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>


int main() {
	pid_t pid;
	pid = fork();
	printf("\n\nHello World!\n");
	if (pid!=0) {
		wait(NULL);
	}
	return 0;
}
