#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>
#include <string.h>

/*
Spec for v0 of walrus-messenger:

Messages are integers. There is a consumer and producer of messages. 
There is also a buffer of fixed size that can hold messages. The producer writes
messages to the buffer and the consumer reads them. If the consumer has read all 
available messages, it stops (by spinlocking when the producer has stopped, or terminating when the final
message has been sent).

If the buffer runs out of space, the producer will overwrite
old messages. Messages are processed FIFO. If the producer stops writing, the consumer
must always eventually reach the most recently posted message (i.e. it wasn't thrown
away, even if the buffer was full). The producer must never block.

The producer continually cycles through the buffer while it has messages to write. It always has a lock
on the cell it will write its next message to. When it writes a message, it acquires a lock on the next
available cell and then unlocks its current cell. The locking is managed by atomically testing a boolean
flag which is represented by a uint32_t. 

The consumer cycles through the buffer, acquiring a lock on a cell, reading its contents, and processing
those contents if the value is not -1. Once it has read the contents, it writes a -1 as a signal value 
that the cell is empty. If it encounters a locked cell (i.e. the cell the producer is currently writing to),
it will cycle back and try to read the producer's most recent message rather than moving forward into old
territory in the buffer. This ensures that the messages are always processed FIFO - otherwise, if the 
producer is pausing before writing more messages, the consumer might pass over the more recent messages and
read old messages out-of-order. As a consequence, any messages to the right of the current cell that the
producer has locked are unreachable and will be lost. (Nb - this approach needs amendment when there are 
multiple producers or consumers.)

Currently this program sends NUM_MESSAGES messages through the producer, to the buffer, and to the 
consumer. The consumer reports an error and kills the program if it detects that it has received messages
out of order, which indicates a serious failure by the program. At the end, it prints how many messages
it successfully read.

The PRODUCER_SLOWDOWN and CONSUMER_SLOWDOWN allow you to make producers and consumers randomly lag during 
operation, to simulate doing expensive operations. Slowing down the consumer leads to far fewer messages
being recieved, as they are overwritten by the faster producer. Slowing down the producer causes all or 
nearly all messages to get read by the consumer. The slowdown works by randomly choosing a number x in the 
range [0, SLOWDOWN] and incrementing an integer from 0 to x in a while loop.
(nb - it is a very low variance randomness. might be interesting to use an exponential distribution instead
of uniform)

*/

#define BUFFER_SIZE 500
#define NUM_MESSAGES 500000
#define PRODUCER_SLOWDOWN 100
#define CONSUMER_SLOWDOWN 0

#define NUM_CONSUMERS 1
#define NUM_PRODUCERS 1



struct message {
	int content;
	uint32_t locked; 
};


int die_immediately=0;
struct message buffer[BUFFER_SIZE];
pthread_t consumer_thread, producer_thread;

void slowdown(int n) {
	int target = (int) (drand48() * n);
	int slow = 0;
	while (slow++ < target) {}
}

/*
 * Prints the contents of the buffer to stdout.
 * Construct a string and then print it rather than printing incrementally,
 * so that the threads don't splatter each other.
 */
void printBuffer() {
	char str[100] = "[";
	char next[100];
	int i;
	for (i=0; i<BUFFER_SIZE; i++) {
		struct message m = buffer[i];
		if (m.locked == 1) {
			strcat(str, "X,");
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
	for (i=0; i<NUM_MESSAGES; i++) {
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
		slowdown(PRODUCER_SLOWDOWN);
		buffer[next].content = i;
		buffer[pos].locked = 0;
		pos = next;
	}
	buffer[pos].locked = 0;

	return NULL;
}

void *consumer(void *_) {
	int pos = 0;
	int val;
	int numRead = 0;
	int lastVal = -1;
	while (1) {
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
		slowdown(CONSUMER_SLOWDOWN);
		buffer[pos].locked = 0;
		buffer[pos].content = -1;
		if (val == NUM_MESSAGES-1 || die_immediately)  {
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
	return 0;
}

