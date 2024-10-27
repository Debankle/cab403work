#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#include <limits.h>

#include "shared.h"

// Linked list of floor queue nodes
typedef struct FloorNode {
    char direction;
    char floor[4];
    struct FloorNode *next;
} FloorNode;

// details for each car in linked list
// connected to each car, and each has
// a destination linked list that they
// move to 
typedef struct Car {
    int fd;
    char name[256];
    char lowest_floor[4];
    char highest_floor[4];
    char current_floor[4];
    char destination_floor[4];
    char status[8];
    FloorNode *floor_queue_head;
    pthread_mutex_t data_mutex;
    pthread_mutex_t queue_mutex;
    struct Car *next;
} Car;

// linked list of calls to handle in order
typedef struct Call {
    char current_floor[4];
    char destination_floor[4];
    int fd;
    struct Call *next;
} Call;

// track status of car connection to clean up on disconnect
typedef struct {
    int conn_fd;
    int *connected;
    pthread_mutex_t *connected_mutex;
} MonitorArgs;

// check how the queue for a car works into three direction blocks
typedef struct {
    char direction;
    FloorNode *start;
    FloorNode *end;
} QueueBlock;

// readonly main listen socket
int sockfd;

// initialise mutex so the linked lists can't be broken
pthread_mutex_t car_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t call_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// update a call has been added so it should be handled
pthread_cond_t call_queue_cond = PTHREAD_COND_INITIALIZER;

// linked list head pointers
Car *car_list_head = NULL;
Call *call_queue_head = NULL;

// Prototype declarations
void *controller_connection(void *args);
void *handle_connection(void *args);
void handle_car_connection(int conn_fd, char *initial_message);
void handle_call_connection(int conn_fd, char *initial_message);
void elevator_control_loop(void);
void sigint_handler(int s);
void *monitor_socket(void *args);
int floor_number(const char *floor);
Car *select_best_car(Call *call);
void insert_floors_into_queue(Car *car, Call *call, char call_direction);
int can_access_floors(Car *car, char *current, char *destination);
void free_floor_queue(FloorNode *head);
int simulate_insertion(Car *car, Call *call, char call_direction);

int main(void) {

    // handle singal interupts
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    // spawn socket listen thread
    pthread_t controller_thread;
    pthread_create(&controller_thread, NULL, controller_connection, NULL);

    // go to handle any calls added
    elevator_control_loop();

    pthread_join(controller_thread, NULL);

    return 0;
}

void elevator_control_loop(void) {
    pthread_mutex_lock(&call_queue_mutex);

    while (1) {
        while (call_queue_head == NULL) {
            pthread_cond_wait(&call_queue_cond, &call_queue_mutex);
        }
        
        // get the oldest call, head of list
        // move next call to the head
        Call *current_call = call_queue_head;
        call_queue_head = call_queue_head->next;
        pthread_mutex_unlock(&call_queue_mutex);

        // get direction call wants to move, up U or down D
        char *from_floor = current_call->current_floor;
        char *to_floor = current_call->destination_floor;
        char call_direction = (floor_number(to_floor) > floor_number(from_floor)) ? 'U' : 'D';

        // pick the best car (didn't implement algorithm, just gets first car in linked list that reaches both floors)
        Car *selected_car = select_best_car(current_call);

        // if car is found add the floors into queue
        // otherwise send back unavailable
        if (selected_car != NULL) {
            pthread_mutex_lock(&selected_car->queue_mutex);
            insert_floors_into_queue(selected_car, current_call, call_direction);
            pthread_mutex_unlock(&selected_car->queue_mutex);

            char response[260];
            snprintf(response, sizeof(response), "CAR %s", selected_car->name);
            send_message(current_call->fd, response);
            close(current_call->fd);
        } else {
            const char *response = "UNAVAILABLE";
            send_message(current_call->fd, response);
            close(current_call->fd);
        }

        // clean up handled call, doesn't need to be here anymore
        free(current_call);

        pthread_mutex_lock(&call_queue_mutex);
    }

    pthread_mutex_unlock(&call_queue_mutex);
}

// iterate over each car in the linked list
// until the first car that has a top and bottom limit that current and destination
// of call can fit inside, assumes that one, no clever algorithm
// either returns that car, or NULL if there are none
Car *select_best_car(Call *call) {
    pthread_mutex_lock(&car_list_mutex);
    Car *current_car = car_list_head;

    while (current_car != NULL) {
        pthread_mutex_lock(&current_car->data_mutex);
        if (can_access_floors(current_car, call->current_floor, call->destination_floor)) {
            pthread_mutex_unlock(&current_car->data_mutex);
            pthread_mutex_unlock(&car_list_mutex);
            break;
        }
        pthread_mutex_unlock(&current_car->data_mutex);
        current_car = current_car->next;
    }
    
    pthread_mutex_unlock(&car_list_mutex);
    
    return current_car;
}

void insert_floors_into_queue(Car *car, Call *call, char call_direction) {
    pthread_mutex_lock(&car->data_mutex);

    char prev_first_floor[4];
    char car_direction;
    int car_current_floor_num = floor_number(car->current_floor);
    int car_destination_floor_num = floor_number(car->destination_floor);

    if (car->floor_queue_head != NULL) {
        strncpy(prev_first_floor, car->floor_queue_head->floor, sizeof(prev_first_floor));
    } else {
        prev_first_floor[0] = '\0';
    }

    if (strcmp(car->status, "Between") == 0) {
        car_direction = (car_destination_floor_num > car_current_floor_num) ? 'U' : 'D';
    } else if (car->floor_queue_head != NULL) {
        int next_floor_num = floor_number(car->floor_queue_head->floor);
        car_direction = (next_floor_num > car_current_floor_num) ? 'U' : 'D';
    } else {
        car_direction = call_direction;
    }

    FloorNode *from_node = malloc(sizeof(FloorNode));
    from_node->direction = call_direction;
    strcpy(from_node->floor, call->current_floor);
    from_node->next = NULL;

    FloorNode *to_node = malloc(sizeof(FloorNode));
    to_node->direction = call_direction;
    strcpy(to_node->floor, call->destination_floor);
    to_node->next = NULL;

    if (car_direction == call_direction) {
        FloorNode *prev = NULL;
        FloorNode *curr = car->floor_queue_head;
        int from_inserted = 0;
        int to_inserted = 0;

        int from_floor_num = floor_number(call->current_floor);
        int to_floor_num = floor_number(call->destination_floor);

        while (curr != NULL && curr->direction == car_direction) {
            int curr_floor_num = floor_number(curr->floor);

            if (!from_inserted && ((car_direction == 'U' && curr_floor_num >= from_floor_num) ||
                                   (car_direction == 'D' && curr_floor_num <= from_floor_num))) {
                if (!(strcmp(curr->floor, call->current_floor) == 0 && curr->direction == call_direction)) {
                    from_node->next = curr;
                    if (prev != NULL) {
                        prev->next = from_node;
                    } else {
                        car->floor_queue_head = from_node;
                    }
                    prev = from_node;
                }
                from_inserted = 1;                
            }

            if (from_inserted && !to_inserted && ((car_direction == 'U' && curr_floor_num >= to_floor_num) ||
                                                  (car_direction == 'D' && curr_floor_num <= to_floor_num))) {
                if (!(strcmp(curr->floor, call->destination_floor) == 0 && curr->direction == call_direction)) {
                    to_node->next = curr;
                    if (prev != NULL) {
                        prev->next = to_node;
                    } else {
                        car->floor_queue_head = to_node;
                    }
                    prev = to_node;
                }
                to_inserted = 1;
                break;
            }

            prev = curr;
            curr = curr->next;            
        }

        if (!from_inserted) {
            if (prev != NULL) {
                prev->next = from_node;
            } else {
                car->floor_queue_head = from_node;
            }
            prev = from_node;
        }
        if (!to_inserted) {
            from_node->next = to_node;
            to_node->next = curr;
        }
    } else {
        if (car->floor_queue_head == NULL) {
            car->floor_queue_head = from_node;
            from_node->next = to_node;
        } else {
            FloorNode *last = car->floor_queue_head;
            while (last->next != NULL) {
                last = last->next;
            }
            last->next = from_node;
            from_node->next = to_node;
        }
    }

    if (car->floor_queue_head != NULL) {
        if ((strcmp(prev_first_floor, car->floor_queue_head->floor) != 0) || (strcmp(car->current_floor, car->floor_queue_head->floor) == 0)) {
            // strncpy(car->destination_floor, car->floor_queue_head->floor, sizeof(car->destination_floor) - 1);
            char message[10];
            snprintf(message, sizeof(message), "FLOOR %s", car->floor_queue_head->floor);
            send_message(car->fd, message);
        }
    }

    pthread_mutex_unlock(&car->data_mutex);
}

// handles controller connection
// implements socket then spawns sub threads to handle each connection
void *controller_connection(void *args __attribute__((unused))) {
    int new_sock;
    struct sockaddr_in controlleraddr, conaddr;
    socklen_t clilen;
    pthread_t tid;

    // make a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    // make socket reusable
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt()");
        exit(EXIT_FAILURE);
    }

    // bind params to socket and sock address
    memset(&controlleraddr, 0, sizeof(controlleraddr));
    controlleraddr.sin_family = AF_INET;
    controlleraddr.sin_port = htons(3000);
    controlleraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&controlleraddr, sizeof(controlleraddr)) < 0) {
        perror("bind()");
        exit(EXIT_FAILURE);
    }

    // listen for connections
    if (listen(sockfd, 50) < 0) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    // if a connection is found make a new thread to handle the new connection
    while (1) {
        clilen = sizeof(conaddr);
        new_sock = accept(sockfd, (struct sockaddr *)&conaddr, &clilen);
        if (new_sock < 0) {
            perror("accept()");
            exit(EXIT_FAILURE);
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

    // clean up socket
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return NULL;
}

// handle a connection
// detach thread so its independent, determine what kind of connection and then make specific thread
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

// handle call connection
// makes new Call node and adds to linked list
// once added, clean up and return
void handle_call_connection(int conn_fd, char *initial_message) {
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
    new_call->fd = conn_fd;
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
    pthread_cond_signal(&call_queue_cond);
    pthread_mutex_unlock(&call_queue_mutex);
}

void handle_car_connection(int conn_fd, char *initial_message) {
    char name[256], lowest_floor[4], highest_floor[4];
    if (sscanf(initial_message, "CAR %255s %3s %3s", name, lowest_floor, highest_floor) != 3) {
        fprintf(stderr, "Invalid CAR message: %s\n", initial_message);
        close(conn_fd);
        return;
    }

    char *status_update = receive_msg(conn_fd);
    if (status_update == NULL) {
        close(conn_fd);
        return;
    }

    char status[8], current_floor[4], destination_floor[4];
    if (sscanf(status_update, "STATUS %7s %3s %3s", status, current_floor, destination_floor) != 3) {
        fprintf(stderr, "Invalid STATUS message: %s\n", status_update);
        close(conn_fd);
        free(status_update);
        return;
    }

    free(status_update);

    Car *new_car = malloc(sizeof(Car));
    if (new_car == NULL) {
        perror("malloc()");
        close(conn_fd);
        return;
    }

    new_car->fd = conn_fd;
    strncpy(new_car->name, name, sizeof(new_car->name) - 1);
    new_car->name[sizeof(new_car->name) - 1] = '\0';
    strncpy(new_car->current_floor, current_floor, sizeof(new_car->current_floor) - 1);
    new_car->current_floor[sizeof(new_car->current_floor) - 1] = '\0';
    strncpy(new_car->destination_floor, destination_floor, sizeof(new_car->destination_floor) - 1);
    new_car->destination_floor[sizeof(new_car->destination_floor) - 1] = '\0';
    strncpy(new_car->lowest_floor, lowest_floor, sizeof(new_car->lowest_floor) - 1);
    new_car->lowest_floor[sizeof(new_car->lowest_floor) - 1] = '\0';
    strncpy(new_car->highest_floor, highest_floor, sizeof(new_car->highest_floor) - 1);
    new_car->highest_floor[sizeof(new_car->highest_floor) - 1] = '\0';
    strncpy(new_car->status, status, sizeof(new_car->status) - 1);
    new_car->status[sizeof(new_car->status) - 1] = '\0';
    new_car->floor_queue_head = NULL;
    pthread_mutex_init(&new_car->data_mutex, NULL);
    pthread_mutex_init(&new_car->queue_mutex, NULL);
    new_car->next = NULL;

    pthread_mutex_lock(&car_list_mutex);
    new_car->next = car_list_head;
    car_list_head = new_car;
    pthread_mutex_unlock(&car_list_mutex);

    // add monitor thread
    int connected = 1;
    pthread_mutex_t connected_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t monitor_thread;
    MonitorArgs *margs = malloc(sizeof(MonitorArgs));
    if (margs == NULL) {
        perror("malloc()");
    } else {
        margs->conn_fd = conn_fd;
        margs->connected = &connected;
        margs->connected_mutex = &connected_mutex;

        if (pthread_create(&monitor_thread, NULL, monitor_socket, (void *)margs) != 0) {
            perror("pthread_create()");
            free(margs);
        }  else {
            pthread_detach(monitor_thread);
        }
    }

    while (1) {
        pthread_mutex_lock(&connected_mutex);
        if (!connected) {
            pthread_mutex_unlock(&connected_mutex);
            break;
        }
        pthread_mutex_unlock(&connected_mutex);

        char *status_update = receive_msg(conn_fd);
        if (status_update == NULL) {
            pthread_mutex_lock(&connected_mutex);
            connected = 0;
            shutdown(conn_fd, SHUT_RDWR);
            close(conn_fd);
            pthread_mutex_unlock(&connected_mutex);
            break;
        }

        if (sscanf(status_update, "STATUS %7s %3s %3s", status, current_floor, destination_floor) != 3) {
            // fprintf(stderr, "Not a status update, disconnecting: %s\n", status_update);
            free(status_update);
            break;
        }
        // fprintf(stdout, "%s: %s\n", new_car->name, status_update);
        free(status_update);

        pthread_mutex_lock(&new_car->data_mutex);
        strncpy(new_car->current_floor, current_floor, sizeof(new_car->current_floor) - 1);
        new_car->current_floor[sizeof(new_car->current_floor) - 1] = '\0';
        strncpy(new_car->destination_floor, destination_floor, sizeof(new_car->destination_floor) - 1);
        new_car->destination_floor[sizeof(new_car->destination_floor) - 1] = '\0';
        strncpy(new_car->status, status, sizeof(new_car->status) - 1);
        new_car->status[sizeof(new_car->status) - 1] = '\0';
        pthread_mutex_unlock(&new_car->data_mutex);

        pthread_mutex_lock(&new_car->queue_mutex);
        if ((strcmp(new_car->status, "Opening") == 0) && (strcmp(new_car->current_floor, new_car->destination_floor) == 0)) {
            if (new_car->floor_queue_head != NULL && (strcmp(new_car->floor_queue_head->floor, new_car->current_floor) == 0)) {
                FloorNode *completed_floor = new_car->floor_queue_head;
                new_car->floor_queue_head = new_car->floor_queue_head->next;
                free(completed_floor);

                if (new_car->floor_queue_head != NULL && strcmp(new_car->floor_queue_head->floor, new_car->current_floor) == 0) {
                    completed_floor = new_car->floor_queue_head;
                    new_car->floor_queue_head = new_car->floor_queue_head->next;
                    free(completed_floor);
                }
                
                if (new_car->floor_queue_head != NULL) {
                    char message[10];
                    snprintf(message, sizeof(message), "FLOOR %s", new_car->floor_queue_head->floor);
                    send_message(new_car->fd, message);
                }
            }
        }
        pthread_mutex_unlock(&new_car->queue_mutex);
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

    pthread_mutex_lock(&connected_mutex);
    if (connected) {
        shutdown(conn_fd, SHUT_RDWR);
        if (close(conn_fd) == -1) {
            perror("close()");
        }
        connected = 0;
    }
    pthread_mutex_unlock(&connected_mutex);

    pthread_mutex_destroy(&new_car->data_mutex);
    pthread_mutex_destroy(&new_car->queue_mutex);
    pthread_mutex_destroy(&connected_mutex);

    free_floor_queue(new_car->floor_queue_head);
    free(new_car);
}

void *monitor_socket(void *args) {
    MonitorArgs *margs = (MonitorArgs *)args;
    int conn_fd = margs->conn_fd;
    int *connected = margs->connected;
    pthread_mutex_t *connected_mutex = margs->connected_mutex;

    struct pollfd pfd;
    pfd.fd = conn_fd;
    pfd.events = POLLIN | POLLERR | POLLHUP;

    while (1) {
        int ret = poll(&pfd, 1, -1);

        if (ret == -1) {
            perror("poll()");
            break;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            pthread_mutex_lock(connected_mutex);
            if (*connected == 1) {
                *connected = 0;
                if (shutdown(conn_fd, SHUT_RDWR) == -1) {
                    perror("shutdown()");
                }   
            }
            pthread_mutex_unlock(connected_mutex);
            break;
        }

        if (pfd.revents & POLLIN) {
            char buf;
            ssize_t res = recv(conn_fd, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
            if (res == 0) {
                pthread_mutex_lock(connected_mutex);
                if (*connected == 1) {
                    *connected = 0;
                    if (shutdown(conn_fd, SHUT_RDWR) == -1) {
                        perror("shutdown()");
                    }
                    close(conn_fd);
                }
                pthread_mutex_unlock(connected_mutex);                
                break;
            } else if (res < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    pthread_mutex_lock(connected_mutex);
                    if (*connected == 1){
                        *connected = 0;
                        if (shutdown(conn_fd, SHUT_RDWR) == -1) {
                            perror("shutdown()");
                        }           
                    }
                    pthread_mutex_unlock(connected_mutex);
                    break;
                }
            }
        }
    }

    free(margs);
    return NULL;
}

void sigint_handler(int s) {
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    pthread_mutex_lock(&car_list_mutex);
    Car *current = car_list_head;
    while (current != NULL) {
        Car *next = current->next;

        shutdown(current->fd, SHUT_RDWR);
        close(current->fd);

        pthread_mutex_lock(&current->queue_mutex);
        free_floor_queue(current->floor_queue_head);
        pthread_mutex_unlock(&current->queue_mutex);

        pthread_mutex_destroy(&current->data_mutex);
        pthread_mutex_destroy(&current->queue_mutex);

        free(current);
        current = next;
    }
    car_list_head = NULL;
    pthread_mutex_unlock(&car_list_mutex);
    pthread_mutex_destroy(&car_list_mutex);

    pthread_mutex_lock(&call_queue_mutex);
    Call *current_call = call_queue_head;
    while (current_call != NULL) {
        Call *next_call = current_call->next;

        close(current_call->fd);
        free(current_call);

        current_call = next_call;
    }
    call_queue_head = NULL;
    pthread_mutex_unlock(&call_queue_mutex);
    pthread_mutex_destroy(&call_queue_mutex);

    pthread_cond_destroy(&call_queue_cond);

    exit(s);
}

int floor_number(const char *floor_str) {
    if (floor_str[0] == 'B') {
        return -atoi(floor_str + 1);
    } else {
        return atoi(floor_str);
    }
}

int can_access_floors(Car *car, char *current, char *destination) {
    if (car == NULL) {
        return 0;
    }
    int ret = (((floor_number(car->lowest_floor) <= floor_number(current)) && 
                (floor_number(car->highest_floor) >= floor_number(current))) &&
                (floor_number(car->lowest_floor) <= floor_number(destination)) && 
                (floor_number(car->highest_floor) >= floor_number(destination))) ? 1 : 0;
    return ret;
}

void free_floor_queue(FloorNode *head) {
    while (head != NULL) {
        FloorNode *temp = head;
        head = head->next;
        free(temp);
    }
}