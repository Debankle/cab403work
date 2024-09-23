#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 512
#define MAX_CLIENTS 50
#define MAX_CLIENT_NAME 50

const char *badName = "Username is too long";

typedef struct client {
    int fd;
    char clientName[MAX_CLIENT_NAME];
    char buffer[BUFFER_SIZE];
    struct sockaddr_storage addr;
} client_t;

typedef struct message {
    time_t time;
    char sender[MAX_CLIENT_NAME];
    char msg[BUFFER_SIZE];
} message_t;

client_t client_sockets[MAX_CLIENTS];
pthread_mutex_t mutex;

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void broadcast_msg(message_t *msg) {
    char time_str[20];
    char full_msg[BUFFER_SIZE + MAX_CLIENT_NAME + 50]; // Buffer to hold the full message
    struct tm *tm_info = localtime(&(msg->time));
    strftime(time_str, sizeof(time_str), "[%H:%M:%S]", tm_info);

    // Build the full message: "[time] sender: message"
    snprintf(full_msg, sizeof(full_msg), "%s %s: %s", time_str, msg->sender, msg->msg);

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i].fd != 0) {
            // Send the full message in one go
            if (send(client_sockets[i].fd, full_msg, strlen(full_msg), 0) == -1) {
                close(client_sockets[i].fd);
                memset(&client_sockets[i], 0, sizeof(client_sockets[i]));
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

void *handle_client(void *ptr) {
    pthread_detach(pthread_self());
    client_t *client = (client_t *)ptr;
    int numBytes;

    numBytes = recv(client->fd, client->buffer, BUFFER_SIZE-1, 0);
    if (numBytes > MAX_CLIENT_NAME-1) {
        send(client->fd, badName, strlen(badName), 0);
        close(client->fd);
        free(client);
        pthread_exit(NULL);
    }
    
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i].fd == 0) {
            client_sockets[i] = *client;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    client->buffer[numBytes] = '\0';
    strcpy(client->clientName, client->buffer);
    printf("Client connected: %s\n", client->clientName);

    while (1) {
        memset(client->buffer, 0, BUFFER_SIZE);
        numBytes = recv(client->fd, client->buffer, BUFFER_SIZE-1, 0);
        if (numBytes <= 0 || strcmp(client->buffer, "\\quit") == 0) {
            printf("Client disconnected: %s\n", client->clientName);
            close(client->fd);

            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i].fd == client->fd) {
                    memset(&client_sockets[i], 0, sizeof(client_sockets[i]));
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
            break;
        }

        client->buffer[numBytes] = '\0';
        message_t *msg = malloc(sizeof(message_t));
        strcpy(msg->msg, client->buffer);
        strcpy(msg->sender, client->clientName);
        msg->time = time(NULL);
        broadcast_msg(msg);
        free(msg);
    }

    free(client);
    pthread_exit(NULL);
}


int main(int argc, char **argv) {
    if (argc != 2) {
        printf("\nUsage: %s <portNo>", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    int serverfd, new_sock;
    struct sockaddr_in serveraddr, clientaddr;
    socklen_t clilen;
    pthread_t tid;

    memset(client_sockets, 0, sizeof(client_sockets));
    pthread_mutex_init(&mutex, NULL);

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        error("Error opening socket");
    }

    int opt = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        error("Error setting socket options");
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serverfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        error("Error on bind");
    }

    listen(serverfd, 5);

    printf("Server started, waiting for connections...\n");

    while (1) {
        clilen = sizeof(clientaddr);
        new_sock = accept(serverfd, (struct sockaddr *)&clientaddr, &clilen);
        if (new_sock < 0) {
            error("Error on accept");
        }

        client_t *new_sock_ptr = malloc(sizeof(client_t));
        new_sock_ptr->fd = new_sock;
        pthread_create(&tid, NULL, handle_client, (void *)new_sock_ptr);
    }

    close(serverfd);
    shutdown(serverfd, SHUT_RDWR);
    pthread_mutex_destroy(&mutex);
    return 0;

}