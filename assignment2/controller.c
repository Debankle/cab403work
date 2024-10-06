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

typedef struct Car {
    int fd;
    char name[256];
    char lowest_floor[4];
    char highest_floor[4];
    char current_floor[4];
    char destination_floor[4];
    char status[8];
    struct Car *next;
} Car;

typedef struct Call {
    char current_floor[4];
    char destination_floor[4];
    struct Call *next;
} Call;


pthread_mutex_t car_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t call_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

Car *car_list_head = NULL;
Call *call_queue_head = NULL;


void *controller_connection(void *args);
void *handle_connection(void *args);
void handle_car_connection(int conn_fd, char *initial_message);
void handle_call_connection(int conn_fd, char *initial_message);
void elevator_control_loop(void);

int main(void) {

    pthread_t controller_thread;
    pthread_create(&controller_thread, NULL, controller_connection, NULL);

    elevator_control_loop();

    pthread_join(controller_thread, NULL);

    return 0;
}

void elevator_control_loop(void) {
    while (1) {
        // Handle elevator logic later
        // Send message; "FLOOR {floor}"
    }
}

void *controller_connection(void *args __attribute__((unused))) {
    int sockfd, new_sock;
    struct sockaddr_in controlleraddr, conaddr;
    socklen_t clilen;
    pthread_t tid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("socket()");
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("setsockopt()");
    }

    memset(&controlleraddr, 0, sizeof(controlleraddr));
    controlleraddr.sin_family = AF_INET;
    controlleraddr.sin_port = htons(3000);
    controlleraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&controlleraddr, sizeof(controlleraddr)) < 0) {
        error("bind()");
    }

    if (listen(sockfd, 50) < 0) {
        error("listen()");
    }

    while (1) {
        clilen = sizeof(conaddr);
        new_sock = accept(sockfd, (struct sockaddr *)&conaddr, &clilen);
        if (new_sock < 0) {
            error("accept()");
        }

        int *new_sock_ptr = malloc(sizeof(int));
        if (new_sock_ptr == NULL) {
            perror("malloc()");
            close(new_sock);
            continue;
        }
        *new_sock_ptr = new_sock;
        pthread_create(&tid, NULL, handle_connection, (void *)new_sock_ptr);
        
    }

    close(sockfd);
    shutdown(sockfd, SHUT_RDWR);
    return NULL;
}

void *handle_connection(void *args) {
    int conn_fd = *((int *) args);
    free(args);
    pthread_detach(pthread_self());

    char* initial_message = receive_msg(conn_fd);
    if (strncmp(initial_message, "CALL", 4) == 0) {
        handle_call_connection(conn_fd, initial_message);
    } else if (strncmp(initial_message, "CAR", 3) == 0) {
        handle_car_connection(conn_fd, initial_message);
    } else {
        fprintf(stderr, "Inavlid initial message: %s\n", initial_message);
        close(conn_fd);
    }

    free(initial_message);

    return NULL;
}

void handle_call_connection(int conn_fd, char *initial_message) {
    fprintf(stdout, "%s", initial_message);
    fflush(stdout);
    char current_floor[4], destination_floor[4];
    if (sscanf(initial_message, "CALL %3s %3s",
               current_floor, destination_floor) != 2) {
        fprintf(stderr, "Invalid CALL message: %s\n", initial_message);
        close(conn_fd);
        return;
    }

    Call *new_call = malloc(sizeof(Call));
    if (new_call == NULL) {
        perror("malloc()");
        close(conn_fd);
        return;
    }
    strncpy(new_call->current_floor, current_floor, sizeof(new_call->current_floor));
    strncpy(new_call->destination_floor, destination_floor, sizeof(new_call->destination_floor));
    new_call->next = NULL;

    pthread_mutex_lock(&call_queue_mutex);

    if (call_queue_head == NULL) {
        call_queue_head = new_call;
    } else {
        Call *temp = call_queue_head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_call;
    }
    pthread_mutex_unlock(&call_queue_mutex);

    close(conn_fd);
}

void handle_car_connection(int conn_fd, char *initial_message) {
    char name[256], lowest_floor[4], highest_floor[4];
    if (sscanf(initial_message, "CAR %255s %3s %3s", name, lowest_floor, highest_floor) != 3) {
        fprintf(stderr, "Invalid CAR message: %s\n", initial_message);
        close(conn_fd);
        return;
    }
    fprintf(stdout, "%s\n", initial_message);

    char *status_update = receive_msg(conn_fd);
    char status[8], current_floor[4], destination_floor[4];
    if (sscanf(status_update, "STATUS %7s %3s %3s", status, current_floor, destination_floor) != 3) {
        fprintf(stderr, "Invalid STATUS message: %s\n", status_update);
        close(conn_fd);
        free(status_update);
        return;
    }
    fprintf(stdout, "%s: %s\n", name, status_update);
    free(status_update);

    Car *new_car = malloc(sizeof(Car));
    if (new_car == NULL) {
        perror("malloc()");
        close(conn_fd);
        return;
    }

    new_car->fd = conn_fd;
    strncpy(new_car->name, name, sizeof(new_car->name));
    strcpy(new_car->current_floor, current_floor);
    strcpy(new_car->destination_floor, destination_floor);
    strcpy(new_car->lowest_floor, lowest_floor);
    strcpy(new_car->highest_floor, highest_floor);
    strcpy(new_car->status, status);
    new_car->next = NULL;

    pthread_mutex_lock(&car_list_mutex);
    new_car->next = car_list_head;
    car_list_head = new_car;
    pthread_mutex_unlock(&car_list_mutex);

    while (1) {
        char *status_update = receive_msg(conn_fd);
        char status[8], current_floor[4], destination_floor[4];
        if (sscanf(status_update, "STATUS %7s %3s %3s", status, current_floor, destination_floor) != 3) {
            fprintf(stderr, "Invalid STATUS message: %s\n", status_update);
            close(conn_fd);
            free(status_update);
            return;
        }
        fprintf(stdout, "%s: %s\n", name, status_update);
        free(status_update);

        pthread_mutex_lock(&car_list_mutex);
        strncpy(new_car->current_floor, current_floor, sizeof(new_car->current_floor));
        strncpy(new_car->destination_floor, destination_floor, sizeof(new_car->destination_floor));
        strncpy(new_car-> status, status, sizeof(new_car->status));
        pthread_mutex_unlock(&car_list_mutex);
    }

    pthread_mutex_lock(&car_list_mutex);

    Car **indirect = &car_list_head;
    while (*indirect != NULL) {
        if (*indirect == new_car) {
            *indirect = new_car->next;
            break;
        }
        indirect = &(*indirect)->next;
    }

    pthread_mutex_unlock(&car_list_mutex);

    close(conn_fd);
    free(new_car);
}