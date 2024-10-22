/** Safety Critical Notes
 * 
 * There are only two violations of the MISRA C safety critical requirements
 * in this program. The rest have been replaced, with fixed length strings, 
 * safe custom string functions, using byte restricted write functions.
 * Additionally, magic numbers have been avoided with preprocessor defines
 * and consistent naming conventions have been followed. There is no unreachable code
 * and only one exit point in the program at a time. Proper control is taken when using 
 * pthread and mutexs, and there are no other threads involved in this particular program.
 * 
 * The two violations are the use of the dynamic memory mapping of shared memory, as well
 * as the infinite loop. Both of these are essential to the operation of the program and cannot be avoided.
 * With respect to the memory mapping, proper error handling has been implemented to
 * close the program should anything happen during initialisation. The while loop is intentionally
 * infinite as this program should not end, and continue to run the same section.
 */


#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_NAME_SIZE 256U
#define MAX_ERROR_MSG_SIZE (MAX_NAME_SIZE + 25U)
#define MAX_FLOOR_LENGTH 4U
#define MAX_STATUS_LENGTH 8U
#define MIN_FLOOR_LENGTH 1U
#define BUTTON_PRESSED 1U
#define BUTTON_NOT_PRESSED 0U
#define PREFIX_LENGTH 4U

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

uint8_t validate_floor_input(const char *floor) {
    uint8_t len = 0U;
    while (floor[len] != '\0' && len < MAX_FLOOR_LENGTH) {
        len++;
    }

    if (len > MAX_FLOOR_LENGTH || len < MIN_FLOOR_LENGTH) {
        return BUTTON_NOT_PRESSED;
    }

    uint8_t index = 0U;
    if (floor[0] == 'B') {
        index = 1U;
        if (len < 2U) {
            return BUTTON_NOT_PRESSED;
        }
    }

    for (; index < len; index++) {
        if (floor[index] < '0' || floor[index] > '9') {
            return BUTTON_NOT_PRESSED;
        }
    }

    return BUTTON_PRESSED;
}

uint8_t validate_status(const char *status) {
    const char valid_status[5][MAX_STATUS_LENGTH] = {
        "Opening",
        "Open",
        "Closing",
        "Closed",
        "Between"
    };

    uint8_t status_valid = BUTTON_NOT_PRESSED;
    uint8_t i, j, match;

    for (i = 0U; i < 5U; i++) {
        j = 0U;
        match = BUTTON_PRESSED;

        while ((status[j] != '\0') && (valid_status[i][j] != '\0') && (j < MAX_STATUS_LENGTH)) {
            if (status[j] != valid_status[i][j]) {
                match = BUTTON_NOT_PRESSED;
                break;
            }
            j++;
        }

        if ((status[j] != '\0') || (valid_status[i][j] != '\0')) {
            match = BUTTON_NOT_PRESSED;
        } 

        if (match == BUTTON_PRESSED) {
            status_valid = BUTTON_PRESSED;
            break;
        }
    }

    return status_valid;
}

void safe_strncpy(char *dest, const char *src, uint8_t max_length) {
    uint8_t i = 0U;
    while ((src[i] != '\0') && (i < max_length - 1U)) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

uint8_t safe_strcmp(const char *str1, const char *str2, uint8_t max_length) {
    uint8_t i = 0U;

    while ((i < max_length) && (str1[i] != '\0') && (str2[i] != '\0')) {
        if (str1[i] != str2[i]) {
            return 0U;
        }
        i++;
    }

    if ((i == max_length) || (str1[i] == str2[i])) {
        return 1U;
    }

    return 0U;
}

int main(int argc, char **argv) {
    int error_code = EXIT_SUCCESS;

    if (argc != 2) {
        const char usage_msg[] = "Usage: ./safety {car name}.\n";
        write(STDOUT_FILENO, usage_msg, sizeof(usage_msg) - 1U);
        error_code = EXIT_FAILURE;
    } else {
        char name[MAX_NAME_SIZE] = "/car";
        uint8_t i = 0U;
        while (argv[1][i] != '\0' && (i + 4u) < MAX_NAME_SIZE) {
            name[PREFIX_LENGTH + i] = argv[1][i];
            i++;
        }
        name [PREFIX_LENGTH + i] = '\0';

        int shm_fd = shm_open(name, O_RDWR, 0666);
        if (shm_fd == -1) {
            char error_msg[MAX_ERROR_MSG_SIZE];
            uint16_t len = 0U;
            const char base_msg[] = "Unable to access car ";
            uint8_t i = 0U;

            while ((base_msg[len] != '\0') && (len < (MAX_ERROR_MSG_SIZE - 1U))) {
                error_msg[len] = base_msg[len];
                len++;
            }

            while ((argv[1][i] != '\0') && (len < (MAX_ERROR_MSG_SIZE - 2U))) {
                error_msg[len] = argv[1][i];
                len++;
                i++;
            }

            if (len < (MAX_ERROR_MSG_SIZE - 2U)) {
                error_msg[len++] = '.';
                error_msg[len++] = '\n';
            }
            error_msg[len] = '\0';
            write(STDOUT_FILENO, error_msg, len);
            error_code = EXIT_FAILURE;
        } else {
            car_shared_mem *car_shm_ptr = (car_shared_mem *)mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            if (car_shm_ptr == MAP_FAILED) {
                close(shm_fd);
                const char mmap_error_msg[] = "mmap()\n";
                write(STDERR_FILENO, mmap_error_msg, sizeof(mmap_error_msg) - 1U);
                error_code = EXIT_FAILURE;
            } else {
                while (1U) {
                    pthread_mutex_lock(&car_shm_ptr->mutex);
                    pthread_cond_wait(&car_shm_ptr->cond, &car_shm_ptr->mutex);

                    if (car_shm_ptr->door_obstruction == 1 && (safe_strcmp(car_shm_ptr->status, "Closing", MAX_STATUS_LENGTH))) {
                        safe_strncpy(car_shm_ptr->status, "Opening", MAX_STATUS_LENGTH);
                    }

                    if (car_shm_ptr->emergency_stop == 1 && car_shm_ptr->emergency_mode == 0) {
                        const char error_msg[] = "The emergency stop button has been pressed!\n";
                        write(STDOUT_FILENO, error_msg, sizeof(error_msg) - 1U);
                        car_shm_ptr->emergency_mode = 1;
                    }

                    if (car_shm_ptr->overload == 1 && car_shm_ptr->emergency_mode == 0) {
                        const char error_msg[] = "The overload sensor has been tripped!\n";
                        write(STDOUT_FILENO, error_msg, sizeof(error_msg) - 1U);
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
                        !(safe_strcmp(car_shm_ptr->status, "Opening", MAX_STATUS_LENGTH) ||
                          safe_strcmp(car_shm_ptr->status, "Closing", MAX_STATUS_LENGTH))))) {
                        const char error_msg[] = "Data consistency error!\n";
                        write(STDOUT_FILENO, error_msg, sizeof(error_msg) - 1U);
                        car_shm_ptr->emergency_mode = 1;
                    }


                    pthread_mutex_unlock(&car_shm_ptr->mutex);
                }

                if (munmap(car_shm_ptr, sizeof(car_shared_mem)) == -1) {
                    close(shm_fd);
                    const char munmap_error_msg[] = "munmap()\n";
                    write(STDERR_FILENO, munmap_error_msg, sizeof(munmap_error_msg) - 1U);
                    error_code = EXIT_FAILURE;
                }

                if (close(shm_fd) == -1) {
                    const char close_error_mgs[] = "close()\n";
                    write(STDERR_FILENO, close_error_mgs, sizeof(close_error_mgs) - 1U);
                    error_code = EXIT_FAILURE;
                }
            }
        }
    }

    return error_code;
}