#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFSIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void recvMsg(int sockfd) {
    char buffer[BUFSIZE];
    int numBytes;

    while (1) {
        memset(buffer, 0, BUFSIZE);
        numBytes = recv(sockfd, buffer, BUFSIZE - 1, 0);
        if (numBytes < 0) {
            error("Error recieving ddata");
        }
        buffer[BUFSIZE] = '\0';

        if (strcmp(buffer, "q") == 0) {
            printf("Client has disconnected.\n");
            break;
        }

        printf("Number of Bytes received from client was %d.\n", numBytes);
        printf("Information sent through socket --> %s\n", buffer);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("\nUsage %s <portNo>\n", argv[1]);
        return 1;
    }

    int fd, clientfd;

    struct sockaddr_in serverAddr;
    socklen_t clilen;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error("Error opening socket");
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(atoi(argv[1]));

    if (bind(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        error("Error on binding");
    }

    struct sockaddr clientAddr;

    listen(fd, 5);
    clilen = sizeof(clientAddr);

    clientfd = accept(fd, (struct sockaddr *)&clientAddr, &clilen);
    if (clientfd < 0) {
        error("Error on accept");
    }

    recvMsg(clientfd);

    close(clientfd);
    close(fd);
    
    return 0;
}