#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#include "shared.h"

typedef struct {
    car_shared_mem *shared_mem_ptr;
    int delay;
    char name[100];
    char lowest_floor[4];
    char highest_floor[4];
} controller_con_args_t;

int createSharedMemoryObject(char *name);
car_shared_mem *mapSharedMemory(int fd);
void elevator_loop(car_shared_mem *shm, int delay);
void *controller_connection(void *arg);

int delay;

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stdout, "Usage: %s {name} {lowest floor} {highest floor} {delay}\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    delay = atoi(argv[4]);

    signal(SIGPIPE, SIG_IGN);

    size_t shm_name_size = strlen(argv[1]) + 5;
    char name[shm_name_size];
    snprintf(name, shm_name_size, "/car%s", argv[1]);

    int car_shm_fd = createSharedMemoryObject(name);
    car_shared_mem *car_shm_ptr = mapSharedMemory(car_shm_fd);
    init_shm(car_shm_ptr, argv[2]);

    controller_con_args_t conn_args = { .shared_mem_ptr = car_shm_ptr };
    strncpy(conn_args.name, argv[1], sizeof(conn_args.name) - 1);
    strncpy(conn_args.lowest_floor, argv[2], sizeof(conn_args.lowest_floor) - 1);
    strncpy(conn_args.highest_floor, argv[3], sizeof(conn_args.highest_floor) - 1);
    conn_args.delay = delay;

    pthread_t connection_thread;
    if (pthread_create(&connection_thread, NULL, controller_connection, &conn_args) != 0) {
        error("pthread_create()");
    }

    elevator_loop(car_shm_ptr, atoi(argv[4]));





    pthread_join(connection_thread, NULL);

    if (munmap(car_shm_ptr, sizeof(car_shared_mem)) == -1) {
        close(car_shm_fd);
        error("munmap()");
    }
    
    if (close(car_shm_fd) == -1) {
        error("close()");
    }

    return 0;
}

int createSharedMemoryObject(char *name) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        error("shm_open()");
    }

    if (ftruncate(fd, sizeof(car_shared_mem)) == -1) {
        close(fd);
        error("ftruncate()");
    }

    return fd;
}

car_shared_mem *mapSharedMemory(int fd) {
    car_shared_mem *shared_mem_ptr = (car_shared_mem *)mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_mem_ptr == MAP_FAILED) {
        close(fd);
        error("mmap()");
    }

    return shared_mem_ptr;
}

void elevator_loop(car_shared_mem *shm, int delay) {
    
}

// Modify to handle incoming connections, send only if delay time is long enough, etc
void *controller_connection(void *args) {
    controller_con_args_t *conn_args = (controller_con_args_t *)args;
    car_shared_mem *shm = conn_args->shared_mem_ptr;

    int sockfd;
    struct sockaddr_in controller_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        error("socket()");
    }

    memset(&controller_addr, 0, sizeof(controller_addr));
    controller_addr.sin_family = AF_INET;
    controller_addr.sin_port = htons(3000);
    if (inet_pton(AF_INET, "127.0.0.1", &controller_addr.sin_addr) != 1) {
        close(sockfd);
        error("inet_pton()");
    }

    while (1) {
        if (connect(sockfd, (struct sockaddr *)&controller_addr, sizeof(controller_addr)) < 0) {
            close(sockfd);
            usleep(conn_args->delay * 1000);
            continue;
        }

        break;
    }

    char initialMessage[256];
    snprintf(initialMessage, sizeof(initialMessage), "CAR %s %s %s", conn_args->name, conn_args->lowest_floor, conn_args->highest_floor);
    send_message(sockfd, initialMessage);

    while (1) {
        pthread_mutex_lock(&shm->mutex);
        char status_message[256];
        snprintf(status_message, sizeof(status_message), "STATUS %s %s %s", shm->status, shm->current_floor, shm->destination_floor);
        pthread_mutex_unlock(&shm->mutex);
        send_message(sockfd, status_message);

        usleep(conn_args->delay * 1000);
    }

    if (shutdown(sockfd, SHUT_RDWR) == -1) {
        close(sockfd);
        error("shutdown()");
    }
    close(sockfd);

    return NULL;
}