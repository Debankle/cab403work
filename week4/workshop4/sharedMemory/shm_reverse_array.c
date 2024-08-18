#include <stdio.h>
#include <stdlib.h>
#include "shm_data.h"
#include "protoHeader.h"
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s shm_name lower_range upper_range count\n", argv[0]);
        return 1;
    }

    if (checkShMemName(argv[1])) {
        fprintf(stderr, "Invalid shm_name: %s\n", argv[1]);
        fprintf(stderr, "usage: %s shm_name lower_range upper_range count\n", argv[0]);
        return 1;
    }

    if (isNumber(argv[2])) {
        fprintf(stderr, "Invalid lower_range: %s (should be an integer)\n", argv[2]);
        fprintf(stderr, "usage: %s shm_name lower_range upper_range count\n", argv[0]);
        return 1;
    }

    if (isNumber(argv[3])) {
        fprintf(stderr, "Invalid upper_range: %s (should be an integer)\n", argv[3]);
        fprintf(stderr, "usage: %s shm_name lower_range upper_range count\n", argv[0]);
        return 1;
    }

    if (isNumber(argv[4])) {
        fprintf(stderr, "Invalid count: %s (should be an integer)\n", argv[4]);
        fprintf(stderr, "usage: %s shm_name lower_range upper_range count\n", argv[0]);
        return 1;
    }

    int count, lower_range, upper_range;
    if (!charToInt(argv[2], &lower_range)) {
        fprintf(stderr, "Error converting lower_range to int\n");
        return 1;
    }
    if (!charToInt(argv[3], &upper_range)) {
        fprintf(stderr, "Error converting upper_range to int\n");
        return 1;
    }
    if (!charToInt(argv[4], &count)) {
        fprintf(stderr, "Error converting count to int\n");
        return 1;
    }

    int fd = createSharedMemoryObject(argv[1], count);
    if (fd == -1) {
        fprintf(stderr, "Failed to create shared memory object\n");
        return 1;
    }

    data_share_t *data = mapSHMObject(fd, count);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Failed to map shared memory object");
        return 1;
    }
    data->arrSize = count;

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // int *random_numbers = (int *)malloc(count * sizeof(int));
        int *random_numbers = genRandomIntegerNumbers(lower_range, upper_range, count);
        if (random_numbers == NULL) {
            fprintf(stderr, "Failed to generate random numbers\n");
            exit(EXIT_FAILURE);
        }
        memcpy(data->arrayData, random_numbers, count * sizeof(int));
        free(random_numbers);
        printArray(pre, data->arrayData, data->arrSize);
        qsort(data->arrayData, data->arrSize, sizeof(int), compareA);
        printArray(postAC, data->arrayData, data->arrSize);
        unMapClose(data, data->arrSize, fd);
        exit(EXIT_SUCCESS);
    } else {
        wait(NULL);
        printArray(postPA, data->arrayData, data->arrSize);
        qsort(data->arrayData, data->arrSize, sizeof(int), compareB);
        printArray(postPD, data->arrayData, data->arrSize);
        unMapClose(data, data->arrSize, fd);
        shm_unlink(argv[1]);
    }

    return 0;
}

/**
 * Function to create the shared memory object and size the shared
 * memory object to the correct length (bytes)
 * \param name - name of the shared memory object e.g. /nameOfSMObject
 * \param arrSize - the size of the array containing the random numbers
 * \returns - file descriptor of shared memory object if success else -1
 */
int createSharedMemoryObject(char *name, int arrSize) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(fd, sizeof(data_share_t) + arrSize) == -1) {
        perror("ftruncate");
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * Function to map the shared memory object to virtual address space of the calling process
 * \param fd - file descriptor returned from creating shared memory object
 * \param arrSize - the length of the array that contains the random integers generated
 * \returns - pointer/address used for mapping that can be used to modify data in shared memory object
 *          - returns MAP_FAILED if the mapping fails
 */
data_share_t *mapSHMObject(int fd, int arrSize) {
    data_share_t *data = (data_share_t *)mmap(NULL, sizeof(data_share_t) + arrSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    return data;
}

/**
 * Function to remove share memory object mapping and close the file descriptor
 * returned from opening the shared memory object
 * \param data - start address for the mapping
 * \param arrSize - the size of the array
 * \param fd - file descriptor returned from shm_open()
 */
void unMapClose(data_share_t *data, int arrSize,int fd) {
    munmap(data, sizeof(data_share_t) + arrSize);
    close(fd);
}