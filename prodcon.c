#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

void *producerThread(void* param);
void *consumerThread(void* param);

int memorySize = 32;
int numOfBlocks = 1;
// the amount of times the producer writes and consumer reads
int times = 2;
unsigned char* sharedMem;
const int blockSize = 32;
const int maximumMemorySize = 64000;
pthread_mutex_t mutex;
sem_t sem[2];


int main(int argc, char *argv[]) {

	// checks amount of args
	if (argc != 3) {
		fprintf(stderr, "ERROR: wrong usage. put in three args");
		return -1;
	}

	if (atoi(argv[1]) < 1) {
		fprintf(stderr, "Argument %d isn't positive\n", atoi(argv[1]));
		return -1;
	}
	else if (atoi(argv[2]) < 1) {
		fprintf(stderr, "Argument %d isn't positive\n", atoi(argv[2]));
		return -1;
	}
	else if (atoi(argv[1]) % 32) {
		fprintf(stderr, "Argument %d isn't divisible by 32\n", atoi(argv[1]));
		return -1;
	}
	else if (atoi(argv[1]) > maximumMemorySize) {
		fprintf(stderr, "Argument %d is over 64k\n", atoi(argv[1]));
		return -1;
	}
	numOfBlocks = memorySize / blockSize;
	times = atoi(argv[2]);
	memorySize = atoi(argv[1]);
	sharedMem = malloc(memorySize);

	// makes the sephamores
	if ((sem_init(&sem[0], 0, 1) == -1) || (sem_init(&sem[1], 0, 0) == -1))
		printf("%s\n", strerror(errno));

	// makes the consumer
	pthread_mutex_init(&mutex, NULL);
	pthread_t producerThreads;
	pthread_t consumerThreads;
	pthread_create(&producerThreads, NULL, producerThread, NULL);
	pthread_create(&consumerThreads, NULL, consumerThread, NULL);

	// waits for the threads to finish
	pthread_join(producerThreads, NULL);
	pthread_join(consumerThreads, NULL);

	if ((sem_destroy(&sem[0]) != 0) || (sem_destroy(&sem[1]) != 0))
		printf("%s\n", strerror(errno));

	// Free up memory
	free(sharedMem);
	pthread_mutex_destroy(&mutex);

	return 0;
}

// producer thread creates 30 bytes of data and stores the checksum in the memory block
// It does it however much the user wants to (inputs)

void *producerThread(void* param) {

	int n;
	for (n = 0; n < times; ++n) {
		// index of the memory region
		int index;

		// waits until the consumer is ready to do it all again
		if (sem_wait(&sem[0]) != 0)
			printf("%s\n", strerror(errno));

		// checks all of the memory blocks
		int theBlock;
		for (theBlock = 0; theBlock < numOfBlocks; theBlock++) {
			pthread_mutex_lock(&mutex);
			unsigned short int checksum = 0;

			for (index = (theBlock*blockSize); index < (((theBlock + 1)*blockSize) - 2); ++index) {
				sharedMem[index] = rand() % 255;
				checksum += sharedMem[index];
			}

			// stores the checksum in the shared memory
			((unsigned short int *)sharedMem)[(index + 1) / 2] = checksum;
			pthread_mutex_unlock(&mutex);

			// unlocks consumer
			if (sem_post(&sem[1]) != 0)
				printf("%s\n", strerror(errno));
		}

	}

	pthread_exit(0);
}


void *consumerThread(void* param) {

	int n;
	for (n = 0; n < times; ++n) {
		int index = 0;

		// goes through the memory
		int block;
		for (block = 0; block < numOfBlocks; block++) {

			// if there's is no data to read then it blocks
			if (sem_wait(&sem[1]) != 0)
				printf("%s\n", strerror(errno));

			pthread_mutex_lock(&mutex);
			unsigned short int actualChecksum = 0;

			// calculates the checksum using the memory
			for (index = (block*blockSize); index < (((block + 1)*blockSize) - 2); ++index) {
				actualChecksum += sharedMem[index];
			}

			// Gets the checksum from the shared memory 
			int initialChecksum = ((unsigned short int *)sharedMem)[(index + 1) / 2];

			// compares with the value in the shared memory.
			// if they don't match then it will show an error
			if (initialChecksum != actualChecksum) {
				printf(" block %d, iteration %d doesn't match\n", block, n);
				printf("Initial Checksum is %d\n Actual Checksum is %d\n", initialChecksum, actualChecksum);
				exit(1);
			}
			pthread_mutex_unlock(&mutex);
		}

		// producer now goes to the next cycle
		if (sem_post(&sem[0]) != 0)
			printf("%s\n", strerror(errno));
	}

	pthread_exit(0);
}
