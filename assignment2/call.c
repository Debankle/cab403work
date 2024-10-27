#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "shared.h"

int main(int argc, char **argv) {    
    if (argc != 3) {
        fprintf(stdout, "Usage: %s {source floor} {destination floor}\n", argv[0]);
        return 0;
    }

    // check whether the source floor is valid
    if (!validate_floor_input(argv[1])) {
        fprintf(stdout, "Invalid floor(s) specified.\n");
        exit(EXIT_FAILURE);
    }

    // check whether the destination floor is valid
    if (!validate_floor_input(argv[2])) {
        fprintf(stdout, "Invalid floor(s) specified.\n");
        exit(EXIT_FAILURE);
    }

    // Already validated, no need to check with strncmp
    if (strcmp(argv[1], argv[2]) == 0) {
        fprintf(stdout, "You are already on that floor!\n");
        exit(EXIT_FAILURE);
    }

    // create a socket object, quit if it fails
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    // make a socket object on port 3000, connect to localhost, quit if it fails
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3000);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        perror("inet_pton()");
        exit(EXIT_FAILURE);
    }

    // Try to connect to a controller, quit if it fails
    if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        fprintf(stdout, "Unable to connect to elevator system.\n");
        exit(EXIT_FAILURE);
    }

    // Maximum length since both inputs are sanitised
    char messageStr[15];
    snprintf(messageStr, sizeof(messageStr), "CALL %s %s", argv[1], argv[2]);
    
    // send call car message to controller
    send_message(sockfd, messageStr);
    
    // wait for controller response to request, ensure its valid, if not quit
    char *recvMessage = receive_msg(sockfd);
    if (strlen(recvMessage) < 1) {
        free(recvMessage);
        perror("receive_msg()");
        exit(EXIT_FAILURE);
    }

    // check if received a car update
    if (recvMessage[0] == 'C') {
        fprintf(stdout, "Car %s is arriving.\n", recvMessage+4);
    } else {
        // otherwise no car could come, only two responses necessary
        fprintf(stdout, "Sorry, no car is available to take this request.\n");
    }

    // clean up allocated memory and quit
    free(recvMessage);

    if (shutdown(sockfd, SHUT_RDWR) == -1) {
        perror("shutdown()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    if (close(sockfd) == -1) {
        perror("close()");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}