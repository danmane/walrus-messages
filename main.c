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
#define FINAL_MESSAGE 10

struct message {
	int content;
	uint32_t locked; 
};

int finished=0;
struct message buffer[BUFFER_SIZE];


void producer() {
	int pos = BUFFER_SIZE - 1;
	buffer[pos].locked = 1;
	int i;
	for (i=0; i<=FINAL_MESSAGE; i++) {
		int next = pos;
		next = (next + 1) % BUFFER_SIZE;
		while (1) {
			if (!OSAtomicTestAndSet(1, &(buffer[next].locked))) {
				break;
			}
			printf(">>%d is locked; continuing\n", next);
		}
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

void consumer() {
	int pos = 0;
	while (1) {
		while (1) {
			pos = (pos + 1) % BUFFER_SIZE;
			if (!OSAtomicTestAndSet(1, &(buffer[pos].locked))) {
				break;
			}
			printf(">>%d is locked; continuing\n", pos);
		}
		printf("read: buffer[%d]=%d\n", pos, buffer[pos].content);
		buffer[pos].locked = 0;
		if (buffer[pos].content == FINAL_MESSAGE)  {
			break;
		}
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
	printBuffer();

	for (i=0; i<BUFFER_SIZE; i++) {
		buffer[i].content = -1;
		buffer[i].locked = 0;
	}


	printBuffer();
	producer();
	consumer();
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