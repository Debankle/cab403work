#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <arpa/inet.h>

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

void error(const char *msg, ...) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

void send_looped(int fd, const void *buf, size_t sz) {
    const char *ptr = (const char *)buf;
    size_t remain = sz;

    while (remain > 0) {
        ssize_t sent = write(fd, ptr, remain);
        if (sent == -1) {
            error("write()");
        }
        ptr += sent;
        remain -= sent;
    }
}

void send_message(int fd, const char *buf) {
    uint32_t len = htonl(strlen(buf));
    send_looped(fd, &len, sizeof(len));
    send_looped(fd, buf, strlen(buf));
}

// TODO: add handle received == 0 for disconnect
void recv_looped(int fd, void *buf, size_t sz) {
    char *ptr = (char *)buf;
    size_t remain = sz;

    while (remain > 0) {
        ssize_t received = read(fd, ptr, remain);
        if (received == -1) {
            error("read()");
        }
        ptr += received;
        remain -= received;
    }
}

char *receive_msg(int fd) {
    uint32_t nlen;
    recv_looped(fd, &nlen, sizeof(nlen));
    uint32_t len = ntohl(nlen);

    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) {
        error("malloc()");
    }
    buf[len] = '\0';
    recv_looped(fd, buf, len);
    return buf;
}

void reset_shm(car_shared_mem *s, const char *floor)
{
  pthread_mutex_lock(&s->mutex);
  size_t offset = offsetof(car_shared_mem, current_floor);
  memset((char *)s + offset, 0, sizeof(*s) - offset);

  strcpy(s->status, "Closed");
  strcpy(s->current_floor, floor);
  strcpy(s->destination_floor, floor);
  pthread_mutex_unlock(&s->mutex);
}

void init_shm(car_shared_mem *s, const char *lowest_floor)
{
  pthread_mutexattr_t mutattr;
  pthread_mutexattr_init(&mutattr);
  pthread_mutexattr_setpshared(&mutattr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&s->mutex, &mutattr);
  pthread_mutexattr_destroy(&mutattr);

  pthread_condattr_t condattr;
  pthread_condattr_init(&condattr);
  pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(&s->cond, &condattr);
  pthread_condattr_destroy(&condattr);

  reset_shm(s, lowest_floor);
}