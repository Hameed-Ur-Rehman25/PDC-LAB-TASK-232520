/*
 * Lab 04 - Task 1: Unsynchronized counter (shared-state hazard)
 * Several threads increment one global counter with no lock and no atomic,
 * so concurrent load/add/store sequences overwrite each other (lost updates).
 */
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 4
#ifndef INCREMENTS_PER_THREAD
#define INCREMENTS_PER_THREAD 1000000
#endif

long counter = 0; /* shared state */

void *worker(void *arg)
{
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        counter++; /* unsynchronized read-modify-write */
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, NULL);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("Expected: %ld\n", (long)NUM_THREADS * INCREMENTS_PER_THREAD);
    printf("Actual:   %ld\n", counter);
    return 0;
}
