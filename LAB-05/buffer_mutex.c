/*
 * Lab 05 - Task 1: Mutex-protected bounded buffer (1 producer, 1 consumer)
 * The buffer is a circular array guarded by one mutex. A thread that finds
 * the buffer full (producer) or empty (consumer) unlocks, counts a retry and
 * yields the CPU, i.e. it polls instead of blocking.
 */
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 8
#endif
#ifndef ITEMS
#define ITEMS 1000000
#endif

static long buffer[BUFFER_SIZE]; /* shared state, guarded by lock */
static int in_pos = 0;           /* next slot to write */
static int out_pos = 0;          /* next slot to read */
static int count = 0;            /* items currently stored */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* Each of these is written by one thread only and read by main after join. */
static long producer_retries = 0;
static long consumer_retries = 0;
static long order_errors = 0;
static long consumed_sum = 0;

static double now_ms(clockid_t clock)
{
    struct timespec ts;
    clock_gettime(clock, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void *producer(void *arg)
{
    for (long item = 0; item < ITEMS; ) {
        pthread_mutex_lock(&lock);
        if (count < BUFFER_SIZE) {
            /* test and insert in the same critical section */
            buffer[in_pos] = item;
            in_pos = (in_pos + 1) % BUFFER_SIZE;
            count++;
            pthread_mutex_unlock(&lock);
            item++;
        } else {
            pthread_mutex_unlock(&lock);
            producer_retries++;
            sched_yield();
        }
    }
    return NULL;
}

static void *consumer(void *arg)
{
    for (long expected = 0; expected < ITEMS; ) {
        pthread_mutex_lock(&lock);
        if (count > 0) {
            long item = buffer[out_pos];
            out_pos = (out_pos + 1) % BUFFER_SIZE;
            count--;
            pthread_mutex_unlock(&lock);
            if (item != expected)
                order_errors++;
            consumed_sum += item;
            expected++;
        } else {
            pthread_mutex_unlock(&lock);
            consumer_retries++;
            sched_yield();
        }
    }
    return NULL;
}

int main(void)
{
    pthread_t prod, cons;
    double wall_start = now_ms(CLOCK_MONOTONIC);
    double cpu_start = now_ms(CLOCK_PROCESS_CPUTIME_ID);

    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons, NULL, consumer, NULL);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    double wall = now_ms(CLOCK_MONOTONIC) - wall_start;
    double cpu = now_ms(CLOCK_PROCESS_CPUTIME_ID) - cpu_start;
    long expected_sum = (long)ITEMS * (ITEMS - 1) / 2;

    printf("[mutex buffer=%d] items=%d checksum=%s order_errors=%ld "
           "producer_retries=%ld consumer_retries=%ld time=%.2f ms cpu=%.2f ms\n",
           BUFFER_SIZE, ITEMS, consumed_sum == expected_sum ? "OK" : "MISMATCH",
           order_errors, producer_retries, consumer_retries, wall, cpu);
    return 0;
}
