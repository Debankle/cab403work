#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void sendMsg(int sockfd) {
    char buffer[BUFSIZE];
    int numBytes;

    while (1) {
        printf("Please enter data to send (q to quit) --> ");
        fgets(buffer, BUFSIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        numBytes = send(sockfd, buffer, strlen(buffer), 0);
        if (numBytes < 0) {
            error("Error sending data");
        }
        printf("Message sent is %s\n", buffer);
        printf("Message length sent = %d\n", numBytes);

        if (strcmp(buffer, "q") == 0) {
            printf("Disconnecting...\n");
            break;
        }
    }
}

int main() {
    int fd;
    struct sockaddr_in serverAddr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error("Error opening socket");
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);

    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
        error("Invalid address");
    }

    if (connect(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        error("Connection Failed");
    }

    sendMsg(fd);

    close(fd);
    return 0;
}