#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "dataqueue.h"
#include "timer.h"
#include <unistd.h>

#define TESTS (800)
#define REPEATS (300)

// contains the numbers
int numbers[TESTS];

dataqueue_t queue;
TickTockTimer_t timer;

volatile int datain;
volatile int err;

void thread_write(void * ctx)
{
	int data[3] = {2, 1, 0};
	int data_size = 3;
	int i;

	for (i = 0; i < TESTS; i++) {
		queue_add(&queue, (void *) data, sizeof(int) * data_size);
		data[2]++; // data 2 contains the test data
		if (!datain) datain = 1;
	}

	pthread_exit(0);
}

void thread_read(void * ctx)
{
	int threadid = *((int *) ctx);

	int * data;
	int data_size;

	int i;
	int read = 0;

	struct timespec pause;
	pause.tv_sec = 0;
	pause.tv_nsec = 1; /* 1ns */

	while (!datain)
		{};

	while (queue_getcurrentsize(&queue) != 0) {

		for (i = 0; i < queue_getcurrentsize(&queue); i++) {
			if (queue_getidandlock(&queue,i,(void **)&data,&data_size)) {
				read++;

				// do stuff with data
				int val = data[2];
				if (val >= 0 && val < TESTS)
					numbers[val]++;
				else {
					err = 1;
					printf("ERROR: Invalid number %d. Reported data size is %d.", val, data_size);
				}

				queue_unlockidandremove(&queue, i);
			}
		}

		// prevents thread starvation
		nanosleep( &pause, NULL );
	}

	printf("Read thread %d read %d items\n", threadid, read);

	pthread_exit(0);
}


long get_mem_usage_in_pages(void)
{
   long s = -1;
   FILE *f = fopen("/proc/self/statm", "r");
   if (!f) return -1;
   fscanf(f, "%ld", &s);
   fclose (f);
   return s;
}

int  main ( int arc, char **argv ) {
	pthread_t thread1, thread2, thread3;

	int repeats = 0;
	err = 0;

	long mem;
	int firstrun = 1;

	timer_tick(&timer);
	while (repeats < REPEATS) {
		printf("--- %d:\n", repeats);

		int i; for (i = 0; i < TESTS; i++) numbers[i] = 0; // initialize the buffer array
		queue_init(&queue);
		datain = 0;

		pthread_create (&thread1, NULL, (void *) &thread_write, NULL);

		int threadid1 = 1; int threadid2 = 2;
		pthread_create (&thread2, NULL, (void *) &thread_read, &threadid1);
		pthread_create (&thread3, NULL, (void *) &thread_read, &threadid2);

		pthread_join(thread1, NULL);
		pthread_join(thread2, NULL);
		pthread_join(thread3, NULL);

		for (i = 0; i < TESTS; i++) if (numbers[i] != 1) {
			err = 1;
			printf("ERROR: Number %d was accessed %d times\n", i, numbers[i]); // initialize the buffer array
		}

		queue_free(&queue);

		if (firstrun) {
			mem = get_mem_usage_in_pages();
			firstrun = 0;
		}

		repeats++;
		if (err) break;
	}
	float runtime = timer_tock(&timer);

	long diff = get_mem_usage_in_pages() - mem;
	if (diff != 0) {
		printf("Possible memory leak\n");
		err = 1;
	}

	printf((err ? "\nERR (%.1f ms per test)\n" : "\nALL TESTS PASSED (%.1f ms per test)\n"), (runtime * 1000.0f) / (float) repeats );
	return 0;
}

