#include "dataqueue.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define LOCKING_ENABLED (1)

void queue_init(dataqueue_t * queue) {
	queue->size = 0;
	queue->packet = NULL;
	queue->packet_sizes = NULL;

#if LOCKING_ENABLED
	pthread_mutex_init(&queue->locker, NULL);
#endif
}

// a copy of the data is made
void queue_add(dataqueue_t * queue, void * data, int size) {
#if LOCKING_ENABLED
	pthread_mutex_lock(&queue->locker); // global lock
#endif

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

#if LOCKING_ENABLED
	pthread_mutex_unlock(&queue->locker);
#endif
}

void queue_add_unsafe(dataqueue_t * queue, void * data, int size) {

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
}

int queue_replace(dataqueue_t * queue, void * data, int size, int id) {
#if LOCKING_ENABLED
	pthread_mutex_lock(&queue->locker);
#endif

	if (id >= queue->size || id < 0) {
#if LOCKING_ENABLED
		pthread_mutex_unlock(&queue->locker);
#endif
		return 0;
	}

	if (size != queue->packet_sizes[id])
		queue->packet[id] = realloc(queue->packet[id], size);

	memcpy(queue->packet[id],data,size);

#if LOCKING_ENABLED
	pthread_mutex_unlock(&queue->locker);
#endif

	return 1;
}

void queue_unlockidandremove(dataqueue_t * queue, int id) {

	int i;
	if (id >= queue->size || id < 0) {
#if LOCKING_ENABLED
		pthread_mutex_unlock(&queue->locker);
#endif
		return;
	};

	const int size = queue->size;

	// free memory
	free(queue->packet[id]);

	// remove it physically
	for (i = id; i < size-1; i++) {
		queue->packet[i] = queue->packet[i+1];
		queue->packet_sizes[i] = queue->packet_sizes[i+1];
	}

	// shrink
	//queue->packet = realloc(queue->packet, sizeof(void *) * (size-1));
	//queue->packet_sizes = (int *) realloc((void *) queue->packet_sizes, sizeof(int) * (size-1));

	queue->size--;

#if LOCKING_ENABLED
	pthread_mutex_unlock(&queue->locker);
#endif
}

void queue_removeunsafe(dataqueue_t * queue, int id) {

	int i;
	if (id >= queue->size || id < 0)
		return;

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
}

int queue_getcurrentsize(dataqueue_t * queue) {
	return queue->size;
}

void queue_lockall(dataqueue_t * queue) {
#if LOCKING_ENABLED
	pthread_mutex_lock(&queue->locker);
#endif
}

void queue_unlockall(dataqueue_t * queue) {
#if LOCKING_ENABLED
	pthread_mutex_unlock(&queue->locker);
#endif
}

int queue_getidunsafe(dataqueue_t * queue, int id, void ** data, int * size) {
	if (id >= queue->size || id < 0) return 0;

	*data = queue->packet[id];
	*size = queue->packet_sizes[id];
	return 1;
}

int queue_getidandlock(dataqueue_t * queue, int id, void ** data, int * size) {
#if LOCKING_ENABLEDrtable_old
	pthread_mutex_lock(&queue->locker);
#endif

	if (id >= queue->size || id < 0) {
#if LOCKING_ENABLED
		pthread_mutex_unlock(&queue->locker);
#endif
		return 0;
	}

	*data = queue->packet[id];
	*size = queue->packet_sizes[id];
	return 1;
}

void queue_unlockid(dataqueue_t * queue, int id) {
#if LOCKING_ENABLED
	pthread_mutex_unlock(&queue->locker);
#endif
}

void queue_purge(dataqueue_t * queue) {
	void * temp_v; int temp_i;

	while (queue->size > 0)
		if (queue_getidandlock(queue, 0, &temp_v, &temp_i))
			queue_unlockidandremove(queue, 0);
}

void queue_purge_unsafe(dataqueue_t * queue) {
	void * temp_v; int temp_i;

	while (queue->size > 0)
		if (queue_getidunsafe(queue, 0, &temp_v, &temp_i))
			queue_removeunsafe(queue, 0);
}

int queue_existsunsafe(dataqueue_t * queue, void * data) {
	int i;
	for (i = 0; i < queue->size; i++)
		if (memcmp(queue->packet[i], data, queue->packet_sizes[i]) == 0)
			return i;
	return -1;
}

void queue_free(dataqueue_t * queue) {

	queue_purge(queue);

#if LOCKING_ENABLED
	pthread_mutex_destroy(&queue->locker);
#endif

	if (queue->packet != NULL) free(queue->packet);
	if (queue->packet_sizes != NULL) free(queue->packet_sizes);
}
