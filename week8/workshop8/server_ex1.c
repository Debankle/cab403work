#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1023

void error(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {

	int clientfd;
	char buffer[BUFFER_SIZE];

	struct sockaddr clientaddr;
	socklen_t addrlen;

	if (argc != 2) {
		printf("\nUsage %s <portNo>\n", argv[0]);
		return 1;
	}

	int port = atoi(argv[1]);

	int fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		error("Socket failed");
	}

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		error("Bind failed");
	}

	listen(fd, 5);

	while (1) {
		clientfd = accept(fd, (struct sockaddr *)&clientaddr, &addrlen);
		if (clientfd < 0) {
			error("Accept failed");
		}

		int length = recv(clientfd, buffer, BUFFER_SIZE, 0);
		buffer[BUFFER_SIZE] = '\0';
		printf("From client - %s", buffer);
		close(clientfd);
	}

	shutdown(fd, SHUT_RDWR);
	close(fd);

	return 0;
}
