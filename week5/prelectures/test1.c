#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *start_routine(void *a) {
	printf("I'm running in a separate thread\n");

	int *x = malloc(sizeof(int));
	*x = 3;
	return (void *)x;
}

int main() {

	pthread_t thread;
	pthread_create(&thread, NULL, start_routine, NULL);
	printf("I'm running in the main thead!\n");
	
	void *output;
	pthread_join(thread, &output);
	printf("The return value is %d\n", *(int *)output);

	return 0;
}
