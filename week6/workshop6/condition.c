#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 5
#define MAX_ITEMS 5

int buffer[BUFFER_SIZE] = {0};
int in = 0;
int out = 0;

pthread_mutex_t mutex;
pthread_cond_t cond_producer;
pthread_cond_t cond_consumer;

void* producer(void* arg) ;
void* consumer(void* arg) ;

void printBuffer() {
	for (int i=0; i < BUFFER_SIZE; i++) {
		printf(" %d ", buffer[i]);
	}
	printf("\n");
}

void* producer(void* arg) {
	for (;;) {
		pthread_mutex_lock(&mutex);

		while (((in + 1) % BUFFER_SIZE) == out) {
			pthread_cond_wait(&cond_producer, &mutex);
		}

		buffer[in] = 1;
		in = (in + 1) % BUFFER_SIZE;
		printBuffer();

		pthread_cond_signal(&cond_consumer);
		pthread_mutex_unlock(&mutex);
		sleep(1);
	}
}

void* consumer(void* arg) {
	for (;;) {
        pthread_mutex_lock(&mutex);

        while (in == out) {
            pthread_cond_wait(&cond_consumer, &mutex);
        }

        buffer[out] = 0;    
        out = (out + 1) % BUFFER_SIZE;
        printBuffer();

        pthread_cond_signal(&cond_producer);

        pthread_mutex_unlock(&mutex);
        sleep(1);
	}
 }

int main(int argc, char *argv[]) {
   pthread_t producerThread1, producerThread2, consumerThread1, consumerThread2;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_producer, NULL);
    pthread_cond_init(&cond_consumer, NULL);

   pthread_create(&producerThread1, NULL, producer, NULL);
   pthread_create(&producerThread2, NULL, producer, NULL);
   pthread_create(&consumerThread1, NULL, consumer, NULL);
   pthread_create(&consumerThread2, NULL, consumer, NULL);

   pthread_join(producerThread1, NULL);
   pthread_join(producerThread1, NULL);
   pthread_join(consumerThread1, NULL);
   pthread_join(consumerThread2, NULL);
   pthread_mutex_destroy(&mutex);

   return 0;
}
