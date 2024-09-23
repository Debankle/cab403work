#include <pthread.h>
#include <stdio.h>
#include <string.h>


void *addMillion(void *arg);
int globalVar = 0;
pthread_mutex_t mut;

int main(int argc, char **argv) {
	pthread_t thread1, thread2;
	pthread_mutex_init(&mut, NULL);
	pthread_create(&thread1, NULL, addMillion, NULL);
	pthread_create(&thread2, NULL, addMillion, NULL);
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	printf("\n\nTotal for globalVar = %d\n", globalVar);
	return 0;
}

void *addMillion(void *ptr) {
	pthread_mutex_lock(&mut);
	for (int i = 0; i < 10000000; i++) {
		globalVar++;
	}
	pthread_mutex_unlock(&mut);

	return NULL;
}
