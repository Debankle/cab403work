#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "shared.h"

bool validate_floor_input(const char *floor) {
    size_t len = strlen(floor);

    if (len > 4 || len < 1) {
        return false;
    }

    char *endptr;
    long floor_number;

    if (floor[0] == 'B') {
        errno = 0;
        floor_number = strtol(floor + 1, &endptr, 10);

        if (errno != 0 || *endptr != '\0' || floor_number < 1 || floor_number > 99) {
            return false;
        }
    } else {
        errno = 0;
        floor_number = strtol(floor, &endptr, 10);

        if (errno != 0 || *endptr != '\0' || floor_number < 1 || floor_number > 999) {
            return false;
        }
    }

    return true;
}

int main(int argc, char **argv) {    
    if (argc != 3) {
        fprintf(stdout, "Usage: %s {source floor} {destination floor}\n", argv[0]);
        return 0;
    }

    if (!validate_floor_input(argv[1])) {
        fprintf(stdout, "Invalid floor(s) specified.\n");
        exit(EXIT_FAILURE);
    }

    if (!validate_floor_input(argv[2])) {
        fprintf(stdout, "Invalid floor(s) specified.\n");
        exit(EXIT_FAILURE);
    }

    // Already validated, no need to check with strncmp
    if (strcmp(argv[1], argv[2]) == 0) {
        fprintf(stdout, "You are already on that floor!\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        error("socket()");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3000);
    const char *ip = "127.0.0.1";
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        error("inet_pton()");
    }

    if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        fprintf(stdout, "Unable to connect to elevator system.\n");
        exit(EXIT_FAILURE);
    }

    // Maximum length since both inputs are sanitised
    char messageStr[15];
    snprintf(messageStr, sizeof(messageStr), "CALL %s %s", argv[1], argv[2]);
    send_message(sockfd, messageStr);
    
    char *recvMessage = receive_msg(sockfd);
    if (strlen(recvMessage) < 1) {
        error("receive_msg()");
    }

    if (recvMessage[0] == 'C') {
        fprintf(stdout, "Car %s is arriving.\n", recvMessage+4);
    } else {
        fprintf(stdout, "Sorry, no car is available to take this request.\n");
    }

    free(recvMessage);
    if (shutdown(sockfd, SHUT_RDWR) == -1) {
        error("shutdown()");
    }
    if (close(sockfd) == -1) {
        error("close()");
    }

    return 0;
}