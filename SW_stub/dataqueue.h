#ifndef DATAQUEUE_H_
#define DATAQUEUE_H_

#include <pthread.h>

// NOTE: Unsafe means that there is no guarantee that the data would not change by the time it is returned (in multithreaded scenario)
// in order to use unsafe functions, first lockall, use the unsafe function and then unlockall

typedef struct dataqueue {
	void ** packet;
	int * packet_sizes;

	pthread_mutex_t locker;

	volatile int size;
} dataqueue_t;

void queue_init(dataqueue_t * queue);

// a copy of the data is made
void queue_add(dataqueue_t * queue, void * data, int size);

void queue_add_unsafe(dataqueue_t * queue, void * data, int size);

int queue_replace(dataqueue_t * queue, void * data, int size, int id);

void queue_unlockidandremove(dataqueue_t * queue, int id);

void queue_removeunsafe(dataqueue_t * queue, int id);

int queue_getcurrentsize(dataqueue_t * queue);

// obtain exclusive access to the data in the id and guarantee that nobody will delete it
int queue_getidandlock(dataqueue_t * queue, int id, void ** data, int * size);

// release exclusive lock and allow other threads to access the id
void queue_unlockid(dataqueue_t * queue, int id);

void queue_free(dataqueue_t * queue);

void queue_purge(dataqueue_t * queue);

void queue_purge_unsafe(dataqueue_t * queue);

void queue_lockall(dataqueue_t * queue);

void queue_unlockall(dataqueue_t * queue);

int queue_getidunsafe(dataqueue_t * queue, int id, void ** data, int * size);

// checks whether data exists and returns its id if it does
int queue_existsunsafe(dataqueue_t * queue, void * data);


#endif
