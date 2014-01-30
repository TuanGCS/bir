#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "dataqueue.h"

#define TESTS (10)

// contains the numbers
int numbers[TESTS];

dataqueue_t queue;

void thread_write(void * ctx)
{
    printf("Write thread starting\n");

	int data[3] = {2, 1, 0};
	int data_size = 3;
	int i;

	for (i = 0; i < TESTS; i++) {
		queue_add(&queue, data, data_size);
		data[2]++; // data 2 contains the test data
	}

	pthread_exit(0);
}

void thread_read(void * ctx)
{
	printf("Read thread starting\n");

	int * data;
	int data_size;

	int test = TESTS;
	int i;

	while (test) {

		for (i = 0; i < TESTS; i++) {
			if (queue_getidandlock(&queue,i,(void *) &data,&data_size)) {
				test--;

				// do stuff with data
				const int val = data[2];
				if (val >= 0 && val < TESTS)
					numbers[val]++;
				else
					printf("ERROR: Invalid number %d received at test %d\n", val, test);

				queue_unlockid(&queue, i);
			}
		}
	}

	pthread_exit(0);
}


int  main ( int arc, char **argv ) {
	int i; for (i = 0; i < TESTS; i++) numbers[i] = 0; // initialize the buffer array
	queue_init(&queue);

	pthread_t thread1, thread2;
	pthread_create (&thread1, NULL, (void *) &thread_write, NULL);
	pthread_create (&thread2, NULL, (void *) &thread_read, NULL);

	pthread_join(thread1, NULL);
	printf("Write thread finished\n");
	pthread_join(thread2, NULL);
	printf("Read thread finished\n");

	for (i = 0; i < TESTS; i++) if (numbers[i] != 1) printf("ERROR: Number %d was accessed %d times\n", i, numbers[i]); // initialize the buffer array

	queue_free(&queue);
    printf("Done\n");
	return 0;
}

