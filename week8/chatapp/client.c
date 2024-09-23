#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 512
#define MAX_CLIENT_NAME 50

int sockfd;
char clientName[MAX_CLIENT_NAME];
char buffer[BUFFER_SIZE];

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void *receive_messages(void *arg) {
    char full_msg[BUFFER_SIZE + MAX_CLIENT_NAME + 50];
    int numBytes;

    while (1) {
        memset(full_msg, 0, sizeof(full_msg));
        numBytes = recv(sockfd, full_msg, sizeof(full_msg) - 1, 0);
        if (numBytes <= 0) {
            printf("Disconnected from server.\n");
            close(sockfd);
            exit(0);
        }

        // Display the full message as it was received
        printf("%s\n", full_msg);
    }
}


int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    struct sockaddr_in server_addr;
    pthread_t recv_thread;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("Error creating socket");
    }

    // Set up the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        error("Invalid address or address not supported");
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Connection failed");
    }

    printf("Connected to the server. Please enter your username: ");
    fgets(clientName, MAX_CLIENT_NAME, stdin);
    clientName[strcspn(clientName, "\n")] = '\0';  // Remove newline character

    // Send the client name (username) to the server
    if (send(sockfd, clientName, strlen(clientName), 0) < 0) {
        error("Error sending client name");
    }

    // Create a thread to handle receiving messages from the server
    pthread_create(&recv_thread, NULL, receive_messages, NULL);

    // Main loop to send messages to the server
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';  // Remove newline character

        if (strcmp(buffer, "\\quit") == 0) {
            printf("Disconnecting...\n");
            close(sockfd);
            exit(0);
        }

        // Send the message to the server
        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            error("Error sending message");
        }
    }

    pthread_join(recv_thread, NULL);
    close(sockfd);

    return 0;
}
