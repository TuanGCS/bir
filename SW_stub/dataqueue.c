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

	const int id = queue->size;

	if (queue->size == 0) {
		queue->packet = (void **) malloc(sizeof(void *) * (queue->size+1));
		queue->packet_sizes = (int *) malloc(sizeof(int) * (queue->size+1));
	} else {
		queue->packet = (void **) realloc((void *) queue->packet, sizeof(void *) * (queue->size+1));
		queue->packet_sizes = (int *) realloc((void *) queue->packet_sizes, sizeof(int) * (queue->size+1));
	}

	// create data and copy it
	void * buf = malloc(size);
	memcpy(buf,data,size);

	// now put it on the right place
	queue->packet[id] = buf;
	queue->packet_sizes[id] = size;

	// announce we have a new element
	queue->size++;

	pthread_mutex_unlock(&queue->locker);
}

int queue_replace(dataqueue_t * queue, void * data, int size, int id) {
	pthread_mutex_lock(&queue->locker);

	if (id >= queue->size || id < 0) {pthread_mutex_unlock(&queue->locker); return 0;}

	if (size != queue->packet_sizes[id])
		queue->packet[id] = realloc(queue->packet[id], size);

	memcpy(queue->packet[id],data,size);

	pthread_mutex_unlock(&queue->locker);

	return 1;
}

void queue_unlockidandremove(dataqueue_t * queue, int id) {

	int i;
	if (id >= queue->size || id < 0) {pthread_mutex_unlock(&queue->locker); return;};

	const int size = queue->size;

	// free memory
	free(queue->packet[id]);

	// remove it physically
	for (i = id; i < size-1; i++) {
		queue->packet[i] = queue->packet[i+1];
		queue->packet_sizes[i] = queue->packet_sizes[i+1];
	}

	// shrink
	queue->packet = realloc(queue->packet, sizeof(void *) * (size-1));
	queue->packet_sizes = (int *) realloc((void *) queue->packet_sizes, sizeof(int) * (size-1));

	queue->size--;

	pthread_mutex_unlock(&queue->locker);
}

int queue_getcurrentsize(dataqueue_t * queue) {
	return queue->size;
}

int queue_getidandlock(dataqueue_t * queue, int id, void ** data, int * size) {
	pthread_mutex_lock(&queue->locker);

	if (id >= queue->size || id < 0) {pthread_mutex_unlock(&queue->locker); return 0;}

	*data = queue->packet[id];
	*size = queue->packet_sizes[id];
	return 1;
}

void queue_unlockid(dataqueue_t * queue, int id) {
	pthread_mutex_unlock(&queue->locker);
}

void queue_purge(dataqueue_t * queue) {
	int i;
	void * temp_v; int temp_i;

	while (queue->size > 0) {
		for (i = 0; i < queue->size; i++)
			if (queue_getidandlock(queue, i, &temp_v, &temp_i))
				queue_unlockidandremove(queue, i);
	}
}

void queue_free(dataqueue_t * queue) {
	int i;

	queue_purge(queue);

	pthread_mutex_destroy(&queue->locker);

	free(queue->packet);
	free(queue->packet_sizes);
}
