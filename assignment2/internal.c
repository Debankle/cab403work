#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "shared.h"

char *floor_above(char *);
char *floor_below(char *);

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stdout, "Usage: %s {car name} {operation}\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    size_t shm_name_size = strlen(argv[1]) + 5;
    char name[shm_name_size];
    snprintf(name, shm_name_size, "/car%s", argv[1]);

    int shm_fd = shm_open(name, O_RDWR, 0666);
    if (shm_fd == -1) {
        fprintf(stdout, "Unable to access car %s.\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    car_shared_mem *car_shm_ptr = (car_shared_mem *)mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (car_shm_ptr == MAP_FAILED) {
        close(shm_fd);
        error("mmap()");
    }

    if (strcmp(argv[2], "open") == 0) {
        pthread_mutex_lock(&car_shm_ptr->mutex);
        car_shm_ptr->open_button = 1;
        pthread_cond_broadcast(&car_shm_ptr->cond);
        pthread_mutex_unlock(&car_shm_ptr->mutex);

    } else if (strcmp(argv[2], "close") == 0) {
        pthread_mutex_lock(&car_shm_ptr->mutex);
        car_shm_ptr->close_button = 1;
        pthread_cond_broadcast(&car_shm_ptr->cond);
        pthread_mutex_unlock(&car_shm_ptr->mutex);

    } else if (strcmp(argv[2], "stop") == 0) {
        pthread_mutex_lock(&car_shm_ptr->mutex);
        car_shm_ptr->emergency_stop = 1;
        pthread_cond_broadcast(&car_shm_ptr->cond);
        pthread_mutex_unlock(&car_shm_ptr->mutex);

    } else if (strcmp(argv[2], "service_on") == 0) {
        pthread_mutex_lock(&car_shm_ptr->mutex);
        car_shm_ptr->individual_service_mode = 1;
        car_shm_ptr->emergency_mode = 0;
        pthread_cond_broadcast(&car_shm_ptr->cond);
        pthread_mutex_unlock(&car_shm_ptr->mutex);

    } else if (strcmp(argv[2], "service_off") == 0) {
        pthread_mutex_lock(&car_shm_ptr->mutex);
        car_shm_ptr->individual_service_mode = 0;
        pthread_cond_broadcast(&car_shm_ptr->cond);
        pthread_mutex_unlock(&car_shm_ptr->mutex);

    } else if (strcmp(argv[2], "up") == 0) {
        pthread_mutex_lock(&car_shm_ptr->mutex);
        if (car_shm_ptr->individual_service_mode) {
            if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                strcpy(car_shm_ptr->destination_floor, floor_above(car_shm_ptr->current_floor));
                pthread_cond_broadcast(&car_shm_ptr->cond);
            } else if (strcmp(car_shm_ptr->status, "Between") == 0) {
                fprintf(stdout, "Operation not allowed while elevator is moving.\n");
            } else {
                fprintf(stdout, "Operation not allowed while doors are open.\n");
            }
        } else {
            fprintf(stdout, "Operation only allowed in service mode.\n");
        }
        pthread_mutex_unlock(&car_shm_ptr->mutex);

    } else if (strcmp(argv[2], "down") == 0) {
        pthread_mutex_lock(&car_shm_ptr->mutex);
        if (car_shm_ptr->individual_service_mode) {
            if (strcmp(car_shm_ptr->status, "Closed") == 0) {
                strcpy(car_shm_ptr->destination_floor, floor_below(car_shm_ptr->current_floor));
                pthread_cond_broadcast(&car_shm_ptr->cond);
            } else if (strcmp(car_shm_ptr->status, "Between") == 0) {
                fprintf(stdout, "Operation not allowed while elevator is moving.\n");
            } else {
                fprintf(stdout, "Operation not allowed while doors are open.\n");
            }
        } else {
            fprintf(stdout, "Operation only allowed in service mode.\n");
        }
        pthread_mutex_unlock(&car_shm_ptr->mutex);

    } else {
        fprintf(stdout, "Invalid operation.\n");
        exit(EXIT_FAILURE);
    }

    if (munmap(car_shm_ptr, sizeof(car_shared_mem)) == -1) {
        close(shm_fd);
        error("munmap()");
    }
    
    if (close(shm_fd) == -1) {
        error("close()");
    }

    return 0;
}

char *floor_above(char *current_floor) {
    static char next_floor[4];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    if (current_floor[0] == 'B') {
        int floor_num = atoi(current_floor + 1);
        if (floor_num == 1) {
            snprintf(next_floor, sizeof(next_floor), "1");
        } else {
            snprintf(next_floor, sizeof(next_floor), "B%d", floor_num - 1);
        }
    } else {
        int floor_num = atoi(current_floor);
        if (floor_num < 999) {
            snprintf(next_floor, sizeof(next_floor), "%d", floor_num + 1);
        } else {
            snprintf(next_floor, sizeof(next_floor), "%s", current_floor);
        }
    }
#pragma GCC diagnostic pop

    return next_floor;
}

char *floor_below(char *current_floor) {
    static char next_floor[4];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    if (current_floor[0] == 'B') {
        int floor_num = atoi(current_floor + 1);
        if ((floor_num) < 99) {
            snprintf(next_floor, sizeof(next_floor), "B%d", floor_num + 1);
        } else {
            snprintf(next_floor, sizeof(next_floor), "%s", current_floor);
        }
    } else {
        int floor_num = atoi(current_floor);
        if (floor_num == 1) {
            snprintf(next_floor, sizeof(next_floor), "B1");
        } else {
            snprintf(next_floor, sizeof(next_floor), "%d", floor_num - 1);
        }
    }
#pragma GCC diagnostic pop

    return next_floor;
}