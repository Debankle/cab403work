#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "shared.h"

// Structure for socket connection thread data
typedef struct {
    char name[100];
    char lowest_floor[4];
    char highest_floor[4];
    int pipe_read_fd;
    int pipe_write_fd;
} controller_con_args_t;

// Structure for socket monitoring thread data
typedef struct {
    int sockfd;
    int *connection_alive;
    pthread_mutex_t *connection_mutex;
} monitor_args_t;

// Prototype definitions
int createSharedMemoryObject(char *name);
car_shared_mem *mapSharedMemory(int fd);
void elevator_loop(int pipe_write_fd);
void *controller_connection(void *arg);
void send_status(int sockfd);
void sigint_handler(int s);
void notify_status_change(int pipe_write_fd);
void calculate_absolute_timeout(struct timespec *ts);
int floor_str_to_int(const char *floor_str);
void floor_int_to_str(int floor_num, char *floor_str);
void move_floor(char *current_floor, const char *destination_floor);
void *monitor_connection(void *args);

// Readonly global cross-thread variables
static int delay; // Delay for car
static int car_shm_fd; // car shared memory file descriptor
static car_shared_mem *car_shm_ptr; // car shared memory pointer object
static char shm_name[256]; // car shared memory name

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stdout, "Usage: %s {name} {lowest floor} {highest floor} {delay}\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // disable signal crashes, either ignore pipe
    // or handle interupt with clean exit function
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    // set delay from arguments
    delay = atoi(argv[4]);

    // Initialise the pipe between main loop and socket
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    // setup shared memory name
    size_t shm_name_size = strlen(argv[1]) + 5;
    snprintf(shm_name, shm_name_size, "/car%s", argv[1]);

    // Initialise shared memory
    car_shm_fd = createSharedMemoryObject(shm_name);
    car_shm_ptr = mapSharedMemory(car_shm_fd);
    init_shm(car_shm_ptr, argv[2]);

    // set up socket thread data
    // pass car data, along with pipe read/write to communicate
    // across threads
    controller_con_args_t conn_args;
    strncpy(conn_args.name, argv[1], sizeof(conn_args.name) - 1);
    strncpy(conn_args.lowest_floor, argv[2], sizeof(conn_args.lowest_floor) - 1);
    strncpy(conn_args.highest_floor, argv[3], sizeof(conn_args.highest_floor) - 1);
    conn_args.pipe_read_fd = pipefd[0];
    conn_args.pipe_write_fd = pipefd[1];

    // Spawn socket connection function on own thread
    pthread_t connection_thread;
    if (pthread_create(&connection_thread, NULL, controller_connection, &conn_args) != 0) {
        perror("pthread_create()");
        exit(EXIT_FAILURE);
    }

    // start the car loop handle function, passing the write pipe
    elevator_loop(pipefd[1]);

    // wait for the socket connection to end and rejoin
    pthread_join(connection_thread, NULL);

    // clean up shared memory objects and exit
    if (munmap(car_shm_ptr, sizeof(car_shared_mem)) == -1) {
        perror("munmap()");
        close(car_shm_fd);
        exit(EXIT_FAILURE);
    }

    if (close(car_shm_fd) == -1) {
        perror("close()");
        exit(EXIT_FAILURE);
    }

    if (shm_unlink(shm_name) == -1) {
        perror("shm_unlink()");
        exit(EXIT_FAILURE);
    }

    return 0;
}

// Create a shared memory object from the 
// name parameter, trucnate to size, and either
// exit or return the file descriptor on success
int createSharedMemoryObject(char *name) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open()");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, sizeof(car_shared_mem)) == -1) {
        perror("ftruncate()");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}

// map the shared memory object to a pointer, either quit on fail or return the pointer
car_shared_mem *mapSharedMemory(int fd) {
    car_shared_mem *shared_mem_ptr = (car_shared_mem *)mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_mem_ptr == MAP_FAILED) {
        perror("mmap()");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return shared_mem_ptr;
}

// elevator_loop: handle the car moving and change state on condition broadcast or timeout
// during each state, iterate over
//  mode
//      return type
//          status or button/floor depending on return type
//
// consider each status combination and handle according to requirements
// if it needs a new delay for the next condition, call calculate_absolute_timeout(&timeout) - resets the delay, for the next status
// if it needs to send a status update, call notify_status_change(pipe_write_fd) - writes to the pipe which is detected in other thread to tell controller
// change the status if needed, reset any buttons every single time. Only one is changed if ret = 0, otherwise reset them all each time
void elevator_loop(int pipe_write_fd) {

    // initialise timeout to track how long each action takes
    struct timespec timeout, current_time;
    int ret;

    // calculate timeout to start countdown on delay
    calculate_absolute_timeout(&timeout);

    pthread_mutex_lock(&car_shm_ptr->mutex);

    while (1) {

        // find out how long previous loop took, if it took longer than the delay instantly return timeout
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        if ((current_time.tv_sec > timeout.tv_sec) ||
            (current_time.tv_sec == timeout.tv_sec && current_time.tv_nsec >= timeout.tv_nsec)) {
            ret = ETIMEDOUT;
        } else {
            // otherwise hold until either condition broadcast or timeout
            // returns ret = 0 for condition broadcast, i.e. something happened earlier than the timeout
            // or ret = ETIMEDOUT if the full delay elapsed
            ret = pthread_cond_timedwait(&car_shm_ptr->cond, &car_shm_ptr->mutex, &timeout);
        }

        // handle emergency mode
        if (car_shm_ptr->emergency_mode == 1) {

            // if the wait triggered early
            if (ret == 0) {

                // if close button pushed, only time it matters is if state is open, won't auto close
                if (car_shm_ptr->close_button == 1) {
                    car_shm_ptr->close_button = 0;
                    if (strcmp(car_shm_ptr->status, "Open") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Closing");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }

                // if open button pushed, only matters if closed or closing, switch to opening
                } else if (car_shm_ptr->open_button == 1) {
                    car_shm_ptr->open_button = 0;
                    if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else if (strcmp(car_shm_ptr->status, "Closing") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                }
            
            // if the full delay elapsed
            } else if (ret == ETIMEDOUT) {

                // if the elevator is between, move to the next floor, set door to closed, and then check if it should open
                // emergency doesn't move beyond finishing a move
                if (strcmp(car_shm_ptr->status, "Between") == 0) {
                    move_floor(car_shm_ptr->current_floor, car_shm_ptr->destination_floor);
                    strcpy(car_shm_ptr->destination_floor, car_shm_ptr->current_floor);
                    strcpy(car_shm_ptr->status, "Closed");
                    notify_status_change(pipe_write_fd);
                    if (car_shm_ptr->open_button == 1) {
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                    }
                    calculate_absolute_timeout(&timeout);
                    car_shm_ptr->open_button = 0;
                    car_shm_ptr->close_button = 0;
                    pthread_cond_broadcast(&car_shm_ptr->cond);

                // if opening, reset open btn, check close button and swap to closing, otherwise finish opening
                } else if (strcmp(car_shm_ptr->status, "Opening") == 0) {
                    car_shm_ptr->open_button = 0;
                    if (car_shm_ptr->close_button == 1) {
                        strcpy(car_shm_ptr->status, "Closing");
                        car_shm_ptr->close_button = 0;
                    } else {
                        strcpy(car_shm_ptr->status, "Open");
                    }
                    calculate_absolute_timeout(&timeout);
                    notify_status_change(pipe_write_fd);
                    pthread_cond_broadcast(&car_shm_ptr->cond);

                // if closing, reset close btn, check open button and swap to opening, otherwise finish closing
                } else if (strcmp(car_shm_ptr->status, "Closing") == 0) {
                    car_shm_ptr->close_button = 0;
                    if (car_shm_ptr->open_button == 1) {
                        strcpy(car_shm_ptr->status, "Opening");
                        car_shm_ptr->open_button = 0;
                    } else {
                        strcpy(car_shm_ptr->status, "Closed");
                    }
                    calculate_absolute_timeout(&timeout);
                    notify_status_change(pipe_write_fd);
                    pthread_cond_broadcast(&car_shm_ptr->cond);

                // if open, reset open btn, check close btn and swap to closing, otherwise stay open
                } else if (strcmp(car_shm_ptr->status, "Open") == 0) {
                    car_shm_ptr->open_button = 0;
                    if (car_shm_ptr->close_button == 1) {
                        car_shm_ptr->close_button = 0;
                        strcpy(car_shm_ptr->status, "Closing");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                    calculate_absolute_timeout(&timeout);
                
                // if closed, reset close btn, check open btn and swap to opening, otherwise stay closed
                } else if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                    car_shm_ptr->close_button = 0;
                    if (car_shm_ptr->open_button == 1) {
                        car_shm_ptr->open_button = 0;
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                    calculate_absolute_timeout(&timeout);
                }
            }

        // handle individual service mode, basically same as emergency mode except it can move
        } else if (car_shm_ptr->individual_service_mode == 1) {

            // handle individual mode and interupt
            if (ret == 0) {

                // if close btn, reset and then if open swap to closing
                if (car_shm_ptr->close_button == 1) {
                    car_shm_ptr->close_button = 0;
                    if (strcmp(car_shm_ptr->status, "Open") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Closing");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }

                // if open btn, reset and then if closed or closing switch to opening
                } else if (car_shm_ptr->open_button == 1) {
                    car_shm_ptr->open_button = 0;
                    if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else if (strcmp(car_shm_ptr->status, "Closing") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                
                // if the current floor is not the destination and closed, switch to between and get ready to move
                } else if (strcmp(car_shm_ptr->current_floor, car_shm_ptr->destination_floor) != 0) {
                    if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Between");
                        notify_status_change(pipe_write_fd);
                    }
                }

            // handle individual mode timeout
            } else if (ret == ETIMEDOUT) {

                // if car is between, move one floor clsoer to destination and close, if destination then begin opening
                // reset any buttons, not relevant
                if (strcmp(car_shm_ptr->status, "Between") == 0) {
                    move_floor(car_shm_ptr->current_floor, car_shm_ptr->destination_floor);
                    strcpy(car_shm_ptr->destination_floor, car_shm_ptr->current_floor);
                    strcpy(car_shm_ptr->status, "Closed");
                    notify_status_change(pipe_write_fd);
                    if (car_shm_ptr->open_button == 1) {
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                    }
                    calculate_absolute_timeout(&timeout);
                    car_shm_ptr->open_button = 0;
                    car_shm_ptr->close_button = 0;
                    pthread_cond_broadcast(&car_shm_ptr->cond);

                // if car is opening, ignore open btn, check close and either switch to closing or open if done
                } else if (strcmp(car_shm_ptr->status, "Opening") == 0) {
                    car_shm_ptr->open_button = 0;
                    if (car_shm_ptr->close_button == 1) {
                        strcpy(car_shm_ptr->status, "Closing");
                        car_shm_ptr->close_button = 0;
                    } else {
                        strcpy(car_shm_ptr->status, "Open");
                    }
                    calculate_absolute_timeout(&timeout);
                    notify_status_change(pipe_write_fd);
                    pthread_cond_broadcast(&car_shm_ptr->cond);

                // if car is closing, check open btn and if so switch to opening, otherwise finish and closed
                } else if (strcmp(car_shm_ptr->status, "Closing") == 0) {
                    car_shm_ptr->close_button = 0;
                    if (car_shm_ptr->open_button == 1) {
                        strcpy(car_shm_ptr->status, "Opening");
                        car_shm_ptr->open_button = 0;
                    } else {
                        strcpy(car_shm_ptr->status, "Closed");
                    }
                    calculate_absolute_timeout(&timeout);
                    notify_status_change(pipe_write_fd);
                    pthread_cond_broadcast(&car_shm_ptr->cond);

                // if open, only thing that can happen is close btn pushed, then switch to closing, otherwise reset delay and do nothing
                } else if (strcmp(car_shm_ptr->status, "Open") == 0) {
                    car_shm_ptr->open_button = 0;
                    if (car_shm_ptr->close_button == 1) {
                        car_shm_ptr->close_button = 0;
                        strcpy(car_shm_ptr->status, "Closing");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                    calculate_absolute_timeout(&timeout);
                
                // if closed, if open btn then begin opening, if difference in floors swap to between, considered moving now
                } else if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                    car_shm_ptr->close_button = 0;
                    if (car_shm_ptr->open_button == 1) {
                        car_shm_ptr->open_button = 0;
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else if (strcmp(car_shm_ptr->current_floor, car_shm_ptr->destination_floor) != 0) {
                        strcpy(car_shm_ptr->status, "Between");
                        notify_status_change(pipe_write_fd);
                    }
                    calculate_absolute_timeout(&timeout);
                }
            }

        // handle normal mode
        } else {

            // if normal mode interupt
            if (ret == 0) {

                // if open btn, switch closing or closed to open
                if (car_shm_ptr->open_button == 1) {
                    car_shm_ptr->open_button = 0;
                    if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else if (strcmp(car_shm_ptr->status, "Closing") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else if (strcmp(car_shm_ptr->status, "Open") == 0) {
                        calculate_absolute_timeout(&timeout);
                    }

                // if close btn, and closed, begin opening
                } else if (car_shm_ptr->close_button == 1) {
                    car_shm_ptr->close_button = 0;
                    if (strcmp(car_shm_ptr->status, "Open") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Closing");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }

                // if difference in floors and closed then switch to between to move
                } else if (strcmp(car_shm_ptr->current_floor, car_shm_ptr->destination_floor) != 0) {
                    if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Between");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }

                // if same floor but no other change and there was a broadcast, destination must have been updated
                // thus we open the doors again, only other case
                } else if (strcmp(car_shm_ptr->current_floor, car_shm_ptr->destination_floor) == 0) {
                    if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                }

            // handle normal mode timeout
            } else if (ret == ETIMEDOUT) {

                // id timeout on between, move one closer and either go to between to keep moving, or start opening
                // if car is at the destination
                if (strcmp(car_shm_ptr->status, "Between") == 0) {
                    move_floor(car_shm_ptr->current_floor, car_shm_ptr->destination_floor);
                    strcpy(car_shm_ptr->status, "Closed");
                    if (strcmp(car_shm_ptr->current_floor, car_shm_ptr->destination_floor) != 0) {
                        strcpy(car_shm_ptr->status, "Between");
                        calculate_absolute_timeout(&timeout);
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else {
                        strcpy(car_shm_ptr->status, "Opening");
                        calculate_absolute_timeout(&timeout);
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                
                // if timeout on closed, nothing happens unless open button pushed or floors aren't the same and thus we start moving
                } else if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                    if (car_shm_ptr->open_button == 1) {
                        car_shm_ptr->open_button = 0;
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Opening");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                    if (strcmp(car_shm_ptr->current_floor, car_shm_ptr->destination_floor) != 0) {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Between");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }

                // if timeout and opening, either finish and be open, or on close btn start closing
                } else if (strcmp(car_shm_ptr->status, "Opening") == 0) {
                    if (car_shm_ptr->close_button == 1) {
                        car_shm_ptr->close_button = 0;
                        strcpy(car_shm_ptr->status, "Closing");
                        calculate_absolute_timeout(&timeout);
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else {
                        strcpy(car_shm_ptr->status, "Open");
                        calculate_absolute_timeout(&timeout);
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                
                // if opening either open btn to keep open or begin closing
                } else if (strcmp(car_shm_ptr->status, "Open") == 0) {
                    if (car_shm_ptr->open_button == 1) {
                        car_shm_ptr->open_button = 0;
                        calculate_absolute_timeout(&timeout);
                    } else if (car_shm_ptr->close_button == 1) {
                        car_shm_ptr->close_button = 0;
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Closing");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else {
                        calculate_absolute_timeout(&timeout);
                        strcpy(car_shm_ptr->status, "Closing");
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }

                // if closing and open btn swap to opening, otherwise finish and be closed
                } else if (strcmp(car_shm_ptr->status, "Closing") == 0) {
                    if (car_shm_ptr->open_button == 1) {
                        car_shm_ptr->open_button = 0;
                        strcpy(car_shm_ptr->status, "Opening");
                        calculate_absolute_timeout(&timeout);
                        notify_status_change(pipe_write_fd);
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    } else {
                        strcpy(car_shm_ptr->status, "Closed");
                        calculate_absolute_timeout(&timeout);
                        notify_status_change(pipe_write_fd);
                        if (strcmp(car_shm_ptr->current_floor, car_shm_ptr->destination_floor) != 0) {
                            strcpy(car_shm_ptr->status, "Between");
                            notify_status_change(pipe_write_fd);
                            calculate_absolute_timeout(&timeout);
                            pthread_cond_broadcast(&car_shm_ptr->cond);
                        }
                        pthread_cond_broadcast(&car_shm_ptr->cond);
                    }
                }
            }
        }

        // loop without unlocking so never miss a cond broadcast

    }

    pthread_mutex_unlock(&car_shm_ptr->mutex);

}

// thread to communicate with controller
// constantly loops sending messages on delay/status change and getting floor instructions
void *controller_connection(void *args) {
    controller_con_args_t *conn_args = (controller_con_args_t *) args;
    int sockfd = -1;
    int connected = 0;

    // pending variable to hold next destination until it can be updated
    char pending_destination[4] = "";

    // infinite loop to try and communicate with controller
    while (1) {

        // make socket object, if it fails wait delay and try again
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            perror("socket()");
            usleep(delay * 1000);
            continue;
        }

        // make controller and bind it to localhost, if fails wait delay and retry
        struct sockaddr_in controller_addr;
        memset(&controller_addr, 0, sizeof(controller_addr));
        controller_addr.sin_family = AF_INET;
        controller_addr.sin_port = htons(3000);
        if (inet_pton(AF_INET, "127.0.0.1", &controller_addr.sin_addr) != 1) {
            perror("inet_pton()");
            close(sockfd);
            sockfd = -1;
            usleep(delay * 1000);
            continue;
        }

        // try to connect to controller. Silenced warning because this will fail if there is no
        // controller so is likely. If fails, loop after delay and retry
        if (connect(sockfd, (struct sockaddr *)&controller_addr, sizeof(controller_addr)) != 0) {
            // perror("connect()");
            close(sockfd);
            sockfd = -1;
            usleep(delay * 1000);
            continue;
        }

        // keep track if socket is connected
        connected = 1;

        // send initial greeting message to controller
        char initial_message[256];
        snprintf(initial_message, sizeof(initial_message), "CAR %s %s %s", conn_args->name, conn_args->lowest_floor, conn_args->highest_floor);
        send_message(sockfd, initial_message);
        send_status(sockfd);

        // setup monitor socket thread to make sure socket is connected
        int connection_alive = 1;
        pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_t monitor_thread;

        // setup monitor connection arguments
        monitor_args_t *margs = malloc(sizeof(monitor_args_t));
        if (margs == NULL) {
            perror("malloc()");
            close(sockfd);
            sockfd = -1;
            connected = 0;
            continue;
        }

        margs->sockfd = sockfd;
        margs->connection_alive = &connection_alive;
        margs->connection_mutex = &connection_mutex;

        if (pthread_create(&monitor_thread, NULL, monitor_connection, (void *)margs) != 0) {
            perror("pthread_create()");
            free(margs);
            close(sockfd);
            sockfd = -1;
            connected = 0;
            continue;
        }

        // setup pollfd to track whethehr can read from each pipe
        struct pollfd fds[2];
        fds[0].fd = sockfd;
        fds[0].events = POLLIN;
        fds[1].fd = conn_args->pipe_read_fd;
        fds[1].events = POLLIN;

        int timeout = delay;

        while (connected) {

            // confirm connection is still active
            pthread_mutex_lock(&connection_mutex);
            if (!connection_alive) {
                pthread_mutex_unlock(&connection_mutex);
                connected = 0;
                break;
            }
            pthread_mutex_unlock(&connection_mutex);

            // check if status is correct to update new destination (not between)
            pthread_mutex_lock(&car_shm_ptr->mutex);
            if (pending_destination[0] != '\0' && strcmp(car_shm_ptr->status, "Between") != 0) {
                strcpy(car_shm_ptr->destination_floor, pending_destination);
                pending_destination[0] = '\0';
                pthread_cond_broadcast(&car_shm_ptr->cond);
            }

            // confirm modes of program so as to break if needed
            // if so quit the connection and go back to retrying connection
            int individual_service_mode = car_shm_ptr->individual_service_mode;
            int emergency_mode = car_shm_ptr->emergency_mode;

            if (emergency_mode == 1) {
                pthread_mutex_unlock(&car_shm_ptr->mutex);
                send_message(sockfd, "EMERGENCY");
                connected = 0;
                break;
            }

            if (individual_service_mode == 1) {
                pthread_mutex_unlock(&car_shm_ptr->mutex);
                send_message(sockfd, "INDIVIDUAL SERVICE");
                connected = 0;
                break;
            }

            pthread_mutex_unlock(&car_shm_ptr->mutex);

            // check what to do, either read from socket or loop pipe, or timeout
            int poll_res = poll(fds, 2, timeout);

            // if error with poll go back and retry connection
            if (poll_res == -1) {
                perror("poll()");
                connected = 0;
                break;
            }

            // timeout, so resent status for usual delayed update
            if (poll_res == 0) {
                send_status(sockfd);
                timeout = delay;
                continue;
            }

            // message on socket, read new floor update
            if (fds[0].revents & POLLIN) {
                char *message = receive_msg(sockfd);
                if (message == NULL) {
                    connected = 0;
                    break;
                }
                if (strncmp(message, "FLOOR ", 6) == 0) {
                    sscanf(message + 6, "%3s", pending_destination);
                }
                free(message);
            }

            // byte on loop pipe, read and then update controller with status change
            if (fds[1].revents & POLLIN) {
                char buf[1];
                ssize_t bytes = read(conn_args->pipe_read_fd, buf, sizeof(buf));
                if (bytes > 0) {
                    send_status(sockfd);
                    timeout = delay;
                } else if (bytes == 0) {
                    connected = 0;
                    break;
                } else {
                    perror("read()");
                    connected = 0;
                    break;
                }
            }

        }

        // cleanup thread and socket
        pthread_mutex_lock(&connection_mutex);
        connection_alive = 0;
        pthread_mutex_unlock(&connection_mutex);

        pthread_join(monitor_thread, NULL);

        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
        connected = 0;

        pthread_mutex_destroy(&connection_mutex);
    }

    close(conn_args->pipe_read_fd);
    free(conn_args);
    return NULL;
}

// infinite loop to monitor the socket pipe
// if it ever drops out disconnect the connection
// and disconnect the thread
void *monitor_connection(void *args) {
    monitor_args_t *margs = (monitor_args_t *)args;
    int sockfd = margs->sockfd;
    int *connection_alive = margs->connection_alive;
    pthread_mutex_t *connection_mutex = margs->connection_mutex;

    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN | POLLERR | POLLHUP;

    while (1) {
        int ret = poll(&pfd, 1, -1);
        if (ret == -1) {
            perror("poll()");
            break;
        }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            pthread_mutex_lock(connection_mutex);
            *connection_alive = 0;
            pthread_mutex_unlock(connection_mutex);
            break;
        }
        if (pfd.revents & POLLIN) {
            char buf;
            ssize_t res = recv(sockfd, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
            if (res == 0) {
                pthread_mutex_lock(connection_mutex);
                *connection_alive = 0;
                pthread_mutex_unlock(connection_mutex);
                break;
            } else if (res < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    pthread_mutex_lock(connection_mutex);
                    *connection_alive = 0;
                    pthread_mutex_unlock(connection_mutex);
                    break;
                }
            }
        }
    }
    free(margs);
    return NULL;
}


// send current status to controller
// assumes mutex lock already enabled
void send_status(int sockfd) {
    char status_message[256];
    snprintf(status_message, sizeof(status_message), "STATUS %s %s %s", car_shm_ptr->status, car_shm_ptr->current_floor, car_shm_ptr->destination_floor);
    send_message(sockfd, status_message);
}

// sigint handler, runs on program crash/fail
// cleans up mapped memory and cleanly exits
void sigint_handler(int s) {
    if (munmap(car_shm_ptr, sizeof(car_shared_mem)) == -1) {
        perror("munmap()");
        exit(EXIT_FAILURE);
    }

    if (close(car_shm_fd) == -1) {
        perror("close()");
        exit(EXIT_FAILURE);
    }

    if (shm_unlink(shm_name) == -1) {
        perror("shm_unlink()");
        exit(EXIT_FAILURE);
    }

    exit(s);
}

// write to the elevator_loop/controller_thread pipe, to let the
// controller connection know it has to update the controller with status change
void notify_status_change(int pipe_write_fd) {
    char buf = '1';
    write(pipe_write_fd, &buf, 1);
}

// reset the delay to delay miliseconds, use nanosecond clock for proper precision
// use realtime clock to avoid losing drift
// set time to delay ms ahead of now, cond_timedwait will wait until it passes ts
void calculate_absolute_timeout(struct timespec *ts) {
    clock_gettime(CLOCK_REALTIME, ts);

    ts->tv_sec += delay / 1000;
    ts->tv_nsec += (delay % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

// move current_floor one floor closer to _destination floor
// get integer representation of each, -ve for Bxx levels, +ve for xxx levels
// find difference and move current by sign, then convert number back to string
void move_floor(char *current_floor, const char *destination_floor) {
    int current = floor_str_to_int(current_floor);
    int destination = floor_str_to_int(destination_floor);
    if (current < destination) {
        current += 1;
    } else if (current > destination) {
        current -= 1;
    } else {
        return;
    }

    floor_int_to_str(current, current_floor);
}

// converet a floor string to integer representative
// B99 - B1 - -ve
// 1- 999 - +ve
int floor_str_to_int(const char *floor_str) {
    if (floor_str[0] == 'B') {
        return -atoi(floor_str + 1);
    } else {
        return atoi(floor_str);
    }
}

// convert number to floor string
// silence gcc complaining, know it will be fixed length here
// if no something has really gone wrong and program deserves to crash
// should be impossible lol
// im really tired hope i can get all these comments done before deadline
// what does happen if snprintf truncates, surely its fine
// just gcc trying to be helpful but -Werror makes it negative fun
void floor_int_to_str(int floor_num, char *floor_str) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    if (floor_num < 0) {
        snprintf(floor_str, 4, "B%d", -floor_num);
    } else {
        snprintf(floor_str, 4, "%d", floor_num);
    }
#pragma GCC diagnostic pop
}