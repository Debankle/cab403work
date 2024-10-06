/**
    Safety critical notes

    dynamic memory: mmap, shm_open, strtol
    variable length array: shm name
    printf and fprintf: bad replace with write
    errno: prohibited
    getter and setter for shared memory
    string functions: get rid of strncpy strcmp
    infinite loop: can't do anything about that
    magic numbers: properly define compare variables
    get rid of boolean
    single exit point, no exits called
    consistent naming convention

 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_NAME_SIZE 256
#define MAX_FLOOR_LENGTH 4
#define MIN_FLOOR_LENGTH 1


typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char current_floor[4];
    char destination_floor[4];
    char status[8];
    uint8_t open_button;
    uint8_t close_button;
    uint8_t door_obstruction;
    uint8_t overload;
    uint8_t emergency_stop;
    uint8_t individual_service_mode;
    uint8_t emergency_mode;
} car_shared_mem;

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

bool validate_status(const char *status) {
    return (strcmp(status, "Opening") == 0 || strcmp(status, "Open") == 0 ||
        strcmp(status, "Closing") == 0 || strcmp(status, "Closed") == 0 ||
        strcmp(status, "Between") == 0);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stdout, "Usage: %s {car name}.\n", argv[0]);
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
        fprintf(stderr, "mmap(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (1) {
        pthread_mutex_lock(&car_shm_ptr->mutex);

        pthread_cond_wait(&car_shm_ptr->cond, &car_shm_ptr->mutex);

        if (car_shm_ptr->door_obstruction == 1 && (strcmp(car_shm_ptr->status, "Closing") == 0)) {
            strncpy(car_shm_ptr->status, "Opening", sizeof(car_shm_ptr->status));
        }

        if (car_shm_ptr->emergency_stop == 1 && car_shm_ptr->emergency_mode == 0) {
            fprintf(stdout, "The emergency stop button has been pressed!\n");
            car_shm_ptr->emergency_mode = 1;
        }

        if (car_shm_ptr->overload == 1 && car_shm_ptr->emergency_mode == 0) {
            fprintf(stdout, "The overload sensor has been tripped!\n");
            car_shm_ptr->emergency_mode = 1;
        }

        if (car_shm_ptr->emergency_mode != 1 && 
            (!validate_floor_input(car_shm_ptr->current_floor) || 
            !validate_floor_input(car_shm_ptr->destination_floor) || 
            !validate_status(car_shm_ptr->status) ||
            car_shm_ptr->open_button > 1 || car_shm_ptr->close_button > 1 ||
            car_shm_ptr->door_obstruction > 1 || car_shm_ptr->overload > 1 ||
            car_shm_ptr->emergency_stop > 1 || car_shm_ptr->individual_service_mode > 1 ||
            car_shm_ptr->emergency_mode > 1 ||
            (car_shm_ptr->door_obstruction == 1 && 
            !(strcmp(car_shm_ptr->status, "Opening") == 0 || strcmp(car_shm_ptr->status, "Closing") == 0)))) {
            fprintf(stdout, "Data consistency error!\n");
            car_shm_ptr->emergency_mode = 1;
        }

        pthread_mutex_unlock(&car_shm_ptr->mutex);
    }

    if (munmap(car_shm_ptr, sizeof(car_shared_mem)) == -1) {
        close(shm_fd);
        perror("munmap()");
    }
    
    if (close(shm_fd) == -1) {
        perror("close()");
    }

    return 0;


    return 0;
}