#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#include "shared.h"

typedef struct connection_args {
    int fd;
    struct sockaddr_storage addr;
} connection_args_t;


void *handle_connection(void *arg);

int main(int argc, char **argv) {

    int sockfd, new_sock;
    struct sockaddr_in controlleraddr, conaddr;
    socklen_t clilen;
    pthread_t tid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("socket()");
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        error("setsockopt()");
    }

    memset(&controlleraddr, 0, sizeof(controlleraddr));
    controlleraddr.sin_family = AF_INET;
    controlleraddr.sin_port = htons(3000);
    controlleraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&controlleraddr, sizeof(controlleraddr)) < 0) {
        error("bind()");
    }

    listen(sockfd, 50);

    while (1) {
        clilen = sizeof(conaddr);
        new_sock = accept(sockfd, (struct sockaddr *)&conaddr, &clilen);
        if (new_sock < 0) {
            error("accept()");
        }

        connection_args_t *new_conn_ptr = malloc(sizeof(connection_args_t));
        new_conn_ptr->fd = new_sock;
        pthread_create(&tid, NULL, handle_connection, (void *)new_conn_ptr);
        
    }

    close(sockfd);
    shutdown(sockfd, SHUT_RDWR);

    return 0;
}

void *handle_connection(void *args) {
    pthread_detach(pthread_self());
    connection_args_t *conn = (connection_args_t *)args;

    char* initial_message = receive_msg(conn->fd);
    if (strncmp(initial_message, "CALL", 4) == 0) {
        // handle call connection
    } else {
        // handle car connection
    }

    return NULL;
}