#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>

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

#define BUFFER_SIZE 4

struct message {
	int content;
	uint32_t locked; 
};

int finished=0;
struct message buffer[BUFFER_SIZE];


void producer() {
	int pos = 0;
	buffer[pos].locked = 1;
	int i;
	for (i=0; i<10; i++) {
		int next = pos;
		next = (next + 1) % BUFFER_SIZE;
		// while (1) {
		// 	if (!OSAtomicTestAndSet(1, &(buffer[next].locked))) {
		// 		break;
		// 	}
		// }
		printf("at i=%d, %d -> %d\n", next, buffer[next].content, i);
		buffer[next].content = i;
		buffer[pos].locked = 0;
		pos = next;
	}
	buffer[pos].locked = 0;
	for (i=0; i<BUFFER_SIZE; i++) {
		printf("\t%d:%d\n", i, buffer[i].content);
	}
}

void printBuffer() {
	int i;
	printf("[");
	for (i=0; i<BUFFER_SIZE; i++) {
		struct message m = buffer[i];
		if (m.locked == 1) {
			printf("X,");
		} else {
			printf("%d,", m.content);
		}
	}
	printf("]\n");
}

int main() {
	int i;
	printf("allocated buffer, have done nothing with it");
	printBuffer();

	printf("assignment sequence 1");
	for (i=0; i<BUFFER_SIZE; i++) {
		buffer[i].content = 99;

	}
	printBuffer();


	printf("assignment sequence 2");
	for (i=0; i<BUFFER_SIZE; i++) {
		struct message* m = &buffer[i];
		m->content = 100;
	}
	printBuffer();
	// producer();
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