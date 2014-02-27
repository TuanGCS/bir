#ifndef DATAQUEUE_H_
#define DATAQUEUE_H_

#include <pthread.h>

typedef struct dataqueue {
	void ** packet;
	int * packet_sizes;

	pthread_mutex_t locker;

	volatile int size;
} dataqueue_t;

void queue_init(dataqueue_t * queue);

// a copy of the data is made
void queue_add(dataqueue_t * queue, void * data, int size);

int queue_replace(dataqueue_t * queue, void * data, int size, int id);

void queue_unlockidandremove(dataqueue_t * queue, int id);

int queue_getcurrentsize(dataqueue_t * queue);

// obtain exclusive access to the data in the id and guarantee that nobody will delete it
int queue_getidandlock(dataqueue_t * queue, int id, void ** data, int * size);

// release exclusive lock and allow other threads to access the id
void queue_unlockid(dataqueue_t * queue, int id);

void queue_free(dataqueue_t * queue);

void queue_purge(dataqueue_t * queue);

void queue_lockall(dataqueue_t * queue);

void queue_unlockall(dataqueue_t * queue);

int queue_getidunsafe(dataqueue_t * queue, int id, void ** data, int * size);


#endif
