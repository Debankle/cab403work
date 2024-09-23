#include <stdio.h>

#define Q 50

typedef struct client {
	int fd;
	char clientName[Q];
} client_t;

int main(void) {
	printf("Size of client_t: %d", sizeof(client_t));
	return 0;
}
