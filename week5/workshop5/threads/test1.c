#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define MESSAGE_REPEAT 2

typedef struct str_thdata {
	int thread_no;
	char message[100];
} thdata;

typedef struct num_thdata {
	int thread_no;
	int sum_to;
} thsum;

void *print_message_function(void *ptr);
void *sum_numbers(void *ptr);

int main(int argc, char **argv) {
	pthread_t thread1, thread2, thread3;
	thdata data1, data2;
	thsum data3;

	data1.thread_no = 1;
	strcpy(data1.message, "Hello! Welcome to Practical 5 - Week 5 already!!!");

	data2.thread_no = 2;
	strcpy(data2.message, "Hi! Week 5- Time flies by when programming in C");

	data3.thread_no = 3;
	data3.sum_to = 20;

	pthread_create(&thread1, NULL, print_message_function, &data1);
	pthread_create(&thread2, NULL, print_message_function, &data2);
	pthread_create(&thread3, NULL, sum_numbers, &data3);

	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);

	return EXIT_SUCCESS;
}

void *print_message_function(void *ptr) {
	thdata *data;
	data = ptr;

	for (int x = 0; x < MESSAGE_REPEAT; x++) {
		printf("\n\nThread %d has the following message -- %s \n", data->thread_no, data->message);
	}
	return NULL;
}

void *sum_numbers(void *ptr) {
	thsum *data;
	data = ptr;
	int totalSum = 0;
	for (int i = 1; i <= data->sum_to; i++) {
		totalSum = totalSum + i;
	}
	printf("\n\nThread number %d reports that the sum of the first %d numbers is = %d", data->thread_no, data->sum_to, totalSum);
}
