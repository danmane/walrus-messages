#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>
#include <string.h>

/*
Spec for v0 of walrus-messenger:

Messages are integers. There is a consumer and producer of messages. 
There is also a buffer of size 3 that can hold messages. The producer writes
messages to the buffer and the consumer reads them. If the consumer has read all 
available messages, it stops. If the buffer runs out of space, the producer will overwrite
old messages. Messages are processed FIFO. If the producer stops writing, the consumer
must always eventually reach the most recently posted message (i.e. it wasn't thrown
away, even if the buffer was full). The producer must never block.

*/

#define BUFFER_SIZE 1000
#define FINAL_MESSAGE 500000
#define SLOWDOWN 0

struct message {
	int content;
	uint32_t locked; 
};

int die_immediately=0;
struct message buffer[BUFFER_SIZE];
pthread_t consumer_thread, producer_thread;

void slowdown(int n) {
	if (n >= 0) {
		int slow = 0;
		while (slow++ < n) {}
	}
}

void printBuffer() {
	char str[100] = "[";
	char next[100];
	int i;
	for (i=0; i<BUFFER_SIZE; i++) {
		struct message m = buffer[i];
		if (m.locked == 1) {
			strcat(str, "X,");
			// printf("X,");
		} else {
			sprintf(next, "%d,", m.content % 100);
			strcat(str, next);
		}
	}
	strcat(str, "]\n");
	printf("%s",str);
}

void *producer(void *_) {
	int pos = BUFFER_SIZE - 1;
	buffer[pos].locked = 1;
	int i;
	for (i=0; i<=FINAL_MESSAGE; i++) {
		slowdown(SLOWDOWN);
		int next = pos;
		next = (next + 1) % BUFFER_SIZE;
		while (1) {
			if (!OSAtomicTestAndSet(1, &(buffer[next].locked))) {
				break;
			}
			// printf(">>>>:  r%d is locked; continuing\n", next);
		}
		// printf(">>>>: at i=%d, %d -> %d\n", next, buffer[next].content % 100, i % 100);
		if (die_immediately) {
			return NULL;
		}
		buffer[next].content = i;
		buffer[pos].locked = 0;
		pos = next;
	}
	buffer[pos].locked = 0;

	// printf(">>>>: producer finished\n");
	return NULL;
}

void *consumer(void *_) {
	int pos = 0;
	int val;
	int numRead = 0;
	int lastVal = -1;
	while (1) {
		slowdown(-SLOWDOWN);
		while (1) {
			pos = (pos + 1) % BUFFER_SIZE;
			// printf("C: pos=%d\n", pos);
			if (!OSAtomicTestAndSet(1, &(buffer[pos].locked))) {
				// printf("C: %d is available; continuing\n", pos);
				break;
			} else {
				// printf("C: %d is locked, retrying\n", pos);
				pos = (pos + BUFFER_SIZE - 2) % BUFFER_SIZE;
			}
		}
		// printf("C: buffer[%d]=%d\n", pos, buffer[pos].content % 100);
		val = buffer[pos].content;
		if (val != -1) {
			if (val <= lastVal) {
				die_immediately = 1;
				printf("ERROR: read buffer[%d]: %d <= lastVal %d\n", pos, val % 100, lastVal % 100);
				printBuffer();
			} else {
				// printf("Read buffer[%d]: %d\n", pos, val % 100);
				// printBuffer("");
				numRead++;
			}
			lastVal = val;

		}
		buffer[pos].locked = 0;
		buffer[pos].content = -1;
		if (val == FINAL_MESSAGE || die_immediately)  {
			printf("numRead: %d\n", numRead);
			break;
		}
	}
	return NULL;
}


int main() {
	int i;

	for (i=0; i<BUFFER_SIZE; i++) {
		buffer[i].content = -1;
		buffer[i].locked = 0;
	}


	// printBuffer("");
	if(pthread_create(&consumer_thread, NULL, consumer, NULL)) {
		fprintf(stderr, "Error creating consumer thread\n");
		return 2;
	}

	if(pthread_create(&producer_thread, NULL, producer, NULL)) {
		fprintf(stderr, "Error creating producer thread\n");
		return 1;
	}
	if(pthread_join(consumer_thread, NULL) || pthread_join(producer_thread, NULL)) {
		fprintf(stderr, "Error joining threads\n");
		return 3;
	}
	// printBuffer("");
	return 0;
}

// void consumer() {
// 	int pos=0;
// 	int lastResult=-1;
// 	int next;
// 	int found;

// 	while (!finished) {

// 		found = 0;
// 		next = pos;
// 		while (!found) {
// 			next = (next + 1) % BUFFER_SIZE;

// 		}

// 	}


// }