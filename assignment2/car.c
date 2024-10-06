#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <poll.h>
#include <time.h>

#include "shared.h"

typedef struct {
    char name[100];
    char lowest_floor[4];
    char highest_floor[4];
    int pipe_read_fd;
    int pipe_write_fd;
} controller_con_args_t;

int createSharedMemoryObject(char *name);
car_shared_mem *mapSharedMemory(int fd);
void elevator_loop(int pipe_write_fd);
void *controller_connection(void *arg);
void send_status(int sockfd);
void sigint_handler(int s);
bool should_wait();
void notify_status_change(int pipe_write_fd);

static int delay;

static int car_shm_fd;
static car_shared_mem *car_shm_ptr;
static char shm_name[256];

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stdout, "Usage: %s {name} {lowest floor} {highest floor} {delay}\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    delay = atoi(argv[4]);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        error("pipe()");
    }

    size_t shm_name_size = strlen(argv[1]) + 5;
    snprintf(shm_name, shm_name_size, "/car%s", argv[1]);

    car_shm_fd = createSharedMemoryObject(shm_name);
    car_shm_ptr = mapSharedMemory(car_shm_fd);
    init_shm(car_shm_ptr, argv[2]);

    controller_con_args_t conn_args;
    strncpy(conn_args.name, argv[1], sizeof(conn_args.name) - 1);
    strncpy(conn_args.lowest_floor, argv[2], sizeof(conn_args.lowest_floor) - 1);
    strncpy(conn_args.highest_floor, argv[3], sizeof(conn_args.highest_floor) - 1);
    conn_args.pipe_read_fd = pipefd[0];
    conn_args.pipe_write_fd = pipefd[1];

    pthread_t connection_thread;
    if (pthread_create(&connection_thread, NULL, controller_connection, &conn_args) != 0) {
        error("pthread_create()");
    }

    elevator_loop(pipefd[1]);

    pthread_join(connection_thread, NULL);

    if (munmap(car_shm_ptr, sizeof(car_shared_mem)) == -1) {
        close(car_shm_fd);
        error("munmap()");
    }
    
    if (close(car_shm_fd) == -1) {
        error("close()");
    }

    if(shm_unlink(shm_name) == -1) {
        error("shm_unlink()");
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

// don't need to worry about  timings
// just do a fresh loop and check the variables each time
// if they change during it actions are not interupted unless emergency
// we can worry about emergency happening later
void elevator_loop(int pipe_write_fd) {
    struct timespec start_time, end_time;
    long elapsed_time;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        pthread_mutex_lock(&car_shm_ptr->mutex);

        while (should_wait()) {
            // check that nothing needs to be done i.e. specific state of doors shut, current_floor is destination_floor, no buttons pushed or whatever
            pthread_cond_wait(&car_shm_ptr->cond, &car_shm_ptr->mutex);
        }

        if (car_shm_ptr->emergency_mode == 1) {
            // handle open close buttons
            notify_status_change(pipe_write_fd);
            pthread_cond_wait(&car_shm_ptr->cond, &car_shm_ptr->mutex);
            pthread_mutex_unlock(&car_shm_ptr->mutex);
            continue;
        }
        
        if (car_shm_ptr->individual_service_mode == 1) {
            // handle individual service mode

            pthread_mutex_unlock(&car_shm_ptr->mutex);
        } else {
            // handle normal mode
            if (car_shm_ptr->open_button == 1) {
                if (strcmp(car_shm_ptr->status, "Open") == 0) {

                } else if (strcmp(car_shm_ptr->status, "Closing") == 0 || strcmp(car_shm_ptr->status, "Closed") == 0) {
                    strcpy(car_shm_ptr->status, "Opening");
                    notify_status_change(pipe_write_fd);
                }
                car_shm_ptr->open_button = 0;
                pthread_cond_broadcast(&car_shm_ptr->cond);
            }
        }

        pthread_mutex_unlock(&car_shm_ptr->mutex);

        clock_gettime(CLOCK_MONOTONIC, &end_time);
        elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000L + (end_time.tv_nsec - start_time.tv_nsec) / 1000L;
        long remaining_time = (delay * 1000L) - elapsed_time;
        if (remaining_time > 0) {
            usleep(remaining_time);
        }
    }
}

// TODO: handle controller disconnect, cleanup reloop etc
// idk if thats necessary but w/e
void *controller_connection(void *args) {
    controller_con_args_t *conn_args = (controller_con_args_t *)args;
    int sockfd = -1;

    struct timeval timer;
    timer.tv_sec = 0;
    timer.tv_usec = delay * 1000;

    while (1) {
        pthread_mutex_lock(&car_shm_ptr->mutex);
        int individual_service_mode = car_shm_ptr->individual_service_mode;
        int emergency_mode = car_shm_ptr->emergency_mode;
\
        if (emergency_mode == 1) {
            printf("in emergency mode why...\n");
            if (sockfd != -1) {
                send_message(sockfd, "EMERGENCY");
                close(sockfd);
                sockfd = -1;
            }
            while (car_shm_ptr->emergency_mode == 1) {
                pthread_cond_wait(&car_shm_ptr->cond, &car_shm_ptr->mutex);
            }
            pthread_mutex_unlock(&car_shm_ptr->mutex);
            continue;
        }

        if (individual_service_mode) {
            printf("in individual mode why...\n");
            if (sockfd != -1) {
                send_message(sockfd, "INDIVIDUAL_SERVICE");
                close(sockfd);
                sockfd = -1;
            }
            while (car_shm_ptr->individual_service_mode == 1) {
                pthread_cond_wait(&car_shm_ptr->cond, &car_shm_ptr->mutex);
            }
            pthread_mutex_unlock(&car_shm_ptr->mutex);
            continue;
        }

        pthread_mutex_unlock(&car_shm_ptr->mutex);

        if (sockfd == -1) {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1) {
                perror("socket()");
                usleep(delay * 1000);
                continue;
            }

            struct sockaddr_in controller_addr;

            memset(&controller_addr, 0, sizeof(controller_addr));
            controller_addr.sin_family = AF_INET;
            controller_addr.sin_port = htons(3000);
            if (inet_pton(AF_INET, "127.0.0.1", &controller_addr.sin_addr) != 1) {
                close(sockfd);
                perror("inet_pton()");
                sockfd = -1;
                usleep(delay * 1000);
                continue;
            }

            if (connect(sockfd, (struct sockaddr *)&controller_addr, sizeof(controller_addr)) != 0) {
                close(sockfd);
                sockfd = -1;
                usleep(delay * 1000);
                continue;
            }

            char initialMessage[256];
            snprintf(initialMessage, sizeof(initialMessage), "CAR %s %s %s", conn_args->name, conn_args->lowest_floor, conn_args->highest_floor);
            send_message(sockfd, initialMessage);
            send_status(sockfd);
            
            timer.tv_sec = 0;
            timer.tv_usec = delay * 1000;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(conn_args->pipe_read_fd, &readfds);
        int maxfd = conn_args->pipe_read_fd;
        if (sockfd != -1) {
            FD_SET(sockfd, &readfds);
            if (sockfd > maxfd) {
                maxfd = sockfd;
            }
        }

        int res = select(maxfd + 1, &readfds, NULL, NULL, &timer);
        if (res == -1) {
            perror("select()");
            break;
        }
        
        if (FD_ISSET(sockfd, &readfds)) {
            char *message = receive_msg(sockfd);
            if (message == NULL) {
                close(sockfd);
                sockfd = -1;
                continue;
            }
            if (strncmp(message, "FLOOR ", 6) == 0) {
                pthread_mutex_lock(&car_shm_ptr->mutex);
                int individual_service_mode = car_shm_ptr->individual_service_mode;
                pthread_mutex_unlock(&car_shm_ptr->mutex); 
            
                if (individual_service_mode == 0) {
                    char new_destination[4];
                    sscanf(message+6, "%s", new_destination);
                    pthread_mutex_lock(&car_shm_ptr->mutex);
                    strcpy(car_shm_ptr->destination_floor, new_destination);
                    pthread_cond_broadcast(&car_shm_ptr->cond);
                    pthread_mutex_unlock(&car_shm_ptr->mutex);
                    send_status(sockfd);
                    timer.tv_sec = 0;
                    timer.tv_usec = delay * 1000;
                }
            }
            free(message);
        }

        if (FD_ISSET(conn_args->pipe_read_fd, &readfds)) {
                char buf[128];
                read(conn_args->pipe_read_fd, buf, sizeof(buf));
                send_status(sockfd);
                timer.tv_sec = 0;
                timer.tv_usec = delay * 1000;
        }
        
        if (res == 0) {
            send_status(sockfd);
            timer.tv_sec = 0;
            timer.tv_usec = delay * 1000;
        }
    }

    if (shutdown(sockfd, SHUT_RDWR) == -1) {
        close(sockfd);
        error("shutdown()");
    }
    close(sockfd);
    close(conn_args->pipe_read_fd);

    return NULL;
}

void send_status(int sockfd) {
    char status_message[256];
    pthread_mutex_lock(&car_shm_ptr->mutex);
    snprintf(status_message, sizeof(status_message), "STATUS %s %s %s", car_shm_ptr->status, car_shm_ptr->current_floor, car_shm_ptr->destination_floor);
    pthread_mutex_unlock(&car_shm_ptr->mutex);
    send_message(sockfd, status_message);
}

void sigint_handler(int s) {
    if (munmap(car_shm_ptr, sizeof(car_shared_mem)) == -1) {
        error("munmap()");
    }

    if (close(car_shm_fd) == -1) {
        error("close()");
    }

    if (shm_unlink(shm_name) == -1) {
        error("shm_unlink()");
    }

    exit(s);
}

bool should_wait() {
    // check if the system is in a state that shouldn't be doing anything, thus can avoid looping and holding mutex
    // updates on broadcast so its fine to hang on this
    // DOES NOT NEED MUTEX LOCK ALREADY IN ONE!!!!
    return false;
}

void notify_status_change(int pipe_write_fd) {
    char buf = '1';
    write(pipe_write_fd, &buf, 1);
}