#include "dataqueue.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

void queue_init(dataqueue_t * queue) {
	queue->size = 0;

	pthread_mutex_init(&queue->locker, NULL);
}

// a copy of the data is made
void queue_add(dataqueue_t * queue, void * data, int size) {
	pthread_mutex_lock(&queue->locker); // global lock

	int i;

	const int id = queue->size;

	// lock all packets
	for (i = 0; i < id; i++)
		pthread_mutex_lock(&queue->packet_locks[i]);

	if (queue->size == 0) {
		queue->packet = malloc(sizeof(void *) * queue->size);
		queue->packet_sizes = (int *) malloc(sizeof(int) * queue->size);
		queue->packet_locks = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t) * queue->size);
	} else {
		queue->packet = realloc(queue->packet, sizeof(void *) * queue->size);
		queue->packet_sizes = (int *) realloc((void *) queue->packet_sizes, sizeof(int) * queue->size);
		queue->packet_locks = (pthread_mutex_t *) realloc((void *) queue->packet_locks, sizeof(pthread_mutex_t) * queue->size);
	}

	// create the new pthread
	pthread_mutex_init(&queue->packet_locks[id], NULL);

	// create data and copy it
	void * buf = malloc(size);
	memcpy(buf,data,size);

	// now put it on the right place
	queue->packet[id] = buf;
	queue->packet_sizes[id] = size;

	// unlock all packets
	for (i = 0; i < id; i++)
		pthread_mutex_unlock(&queue->packet_locks[i]);

	// announce we have a new element
	queue->size++;

	pthread_mutex_unlock(&queue->locker);
}

void queue_remove(dataqueue_t * queue, int id) {
	int i;
	if (id >= queue->size || id < 0) return;

	pthread_mutex_lock(&queue->locker); // global lock
	const int size = queue->size;

	// lock all packets
	for (i = 0; i < size; i++)
		pthread_mutex_lock(&queue->packet_locks[i]);

	// free memory
	free(queue->packet[id]);
	pthread_mutex_destroy(&queue->packet_locks[id]);

	// remove it physically
	for (i = id; i < size-1; i++) {
		queue->packet[i] = queue->packet[i+1];
		queue->packet_sizes[i] = queue->packet_sizes[i+1];
		queue->packet_locks[i] = queue->packet_locks[i+1];
	}

	// shrink
	queue->packet = realloc(queue->packet, sizeof(void *) * (size-1));
	queue->packet_sizes = (int *) realloc((void *) queue->packet_sizes, sizeof(int) * (size-1));
	queue->packet_locks = (pthread_mutex_t *) realloc((void *) queue->packet_locks, sizeof(pthread_mutex_t) * (size-1));

	queue->size--;
	for (i = 0; i < queue->size; i++)
		pthread_mutex_unlock(&queue->packet_locks[i]);

	pthread_mutex_unlock(&queue->locker);
}

int queue_getcurrentsize(dataqueue_t * queue) {
	return queue->size;
}

int queue_getidandlock(dataqueue_t * queue, int id, void ** data, int * size) {
	if (id >= queue->size || id < 0) return 0;
	pthread_mutex_lock(&queue->packet_locks[id]);

	*data = queue->packet[id];
	*size = queue->packet_sizes[id];
	return 1;
}

void queue_unlockid(dataqueue_t * queue, int id) {
	pthread_mutex_unlock(&queue->packet_locks[id]);
}

void queue_free(dataqueue_t * queue) {
	int i;
	pthread_mutex_lock(&queue->locker); // global lock

	for (i = 0; i < queue->size; i++) queue_remove(queue, i);

	pthread_mutex_destroy(&queue->locker);

	free(queue->packet);
	free(queue->packet_sizes);
	free(queue->packet_locks);
}
