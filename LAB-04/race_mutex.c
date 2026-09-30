/*
 * Lab 04 - Task 3: Fixing the race with a mutex (critical section)
 * Only the counter++ statement is inside the critical section, so at most
 * one thread performs the load/add/store at a time.
 */
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 4
#ifndef INCREMENTS_PER_THREAD
#define INCREMENTS_PER_THREAD 1000000
#endif

long counter = 0; /* shared state */
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg)
{
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        pthread_mutex_lock(&counter_lock);
        counter++; /* critical section */
        pthread_mutex_unlock(&counter_lock);
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

    /* Safe without the lock: pthread_join() makes the workers' writes visible. */
    printf("Expected: %ld\n", (long)NUM_THREADS * INCREMENTS_PER_THREAD);
    printf("Actual:   %ld\n", counter);
    return 0;
}
