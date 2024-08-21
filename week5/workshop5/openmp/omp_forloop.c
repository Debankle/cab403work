#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	omp_set_num_threads(4);
	int val[8];

#pragma omp parallel default(none) shared(val)
	{
#pragma omp for
		for (int i = 1; i < 9; i++) {
			val[i-1] = i * i * i;
			int thread = omp_get_thread_num();

			printf("%d^3=%d, using thread %d\n", i, val[i-1], thread);
		}
	}
}
