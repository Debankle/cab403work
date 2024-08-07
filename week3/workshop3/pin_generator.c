#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

int main() {
	pid_t childpid;
	int fd[2];
	char buf[5];
	int pin;

	if (pipe(fd) == -1) {
		perror("Pipe");
		exit(EXIT_FAILURE);
	}

	if ((childpid = fork()) == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (childpid != 0) {
		close(fd[1]);

		printf("Waiting for PIN...\n");

		ssize_t bytesRead = read(fd[0], buf,4);
		if (bytesRead == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		buf[4] = '\0';

		printf("Bytes read: %ld\n", bytesRead);
		printf("PIN: %s\n", buf);

		close(fd[0]);
	} else {
		close(fd[0]);

		srand(getppid() + getpid());

		pin = rand() % 10000;

		snprintf(buf, sizeof(buf), "%04d", pin);
		if (write(fd[1], buf, strlen(buf)) == -1) {
			perror("write");
			exit(EXIT_FAILURE);
		}

		close(fd[1]);
		exit(0);
	}

	return 0;
}
