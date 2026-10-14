/*
 * Lab 05 - Task 2: Bounded buffer with several producers and consumers
 * Two counting semaphores do the waiting: empty_slots counts free slots and
 * full_slots counts stored items. buf_lock only protects the indices.
 * Order in both threads: semaphore wait, mutex lock, update, unlock, post.
 *   -DSWAP_LOCK_ORDER  deadlock experiment: the consumer takes buf_lock
 *                      before waiting on full_slots
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#ifndef NUM_PRODUCERS
#define NUM_PRODUCERS 2
#endif
#ifndef NUM_CONSUMERS
#define NUM_CONSUMERS 2
#endif
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 8
#endif
#ifndef ITEMS_PER_PRODUCER
#define ITEMS_PER_PRODUCER 100000
#endif

#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)
#define STOP_ITEM (-1L) /* tells one consumer to exit */

static long buffer[BUFFER_SIZE]; /* shared state, guarded by buf_lock */
static int in_pos = 0;
static int out_pos = 0;
static pthread_mutex_t buf_lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t empty_slots; /* free slots, starts at BUFFER_SIZE */
static sem_t full_slots;  /* stored items, starts at 0 */

/* seen[id] = how many times item id was consumed (must end up as 1) */
static unsigned char seen[TOTAL_ITEMS];

static double now_ms(clockid_t clock)
{
    struct timespec ts;
    clock_gettime(clock, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void put_item(long item)
{
    sem_wait(&empty_slots); /* blocks while the buffer is full */
    pthread_mutex_lock(&buf_lock);
    buffer[in_pos] = item;
    in_pos = (in_pos + 1) % BUFFER_SIZE;
    pthread_mutex_unlock(&buf_lock);
    sem_post(&full_slots);
}

static long take_item(void)
{
#ifdef SWAP_LOCK_ORDER
    pthread_mutex_lock(&buf_lock); /* wrong: may sleep while holding the lock */
    sem_wait(&full_slots);
#else
    sem_wait(&full_slots); /* blocks while the buffer is empty */
    pthread_mutex_lock(&buf_lock);
#endif
    long item = buffer[out_pos];
    out_pos = (out_pos + 1) % BUFFER_SIZE;
    pthread_mutex_unlock(&buf_lock);
    sem_post(&empty_slots);
    return item;
}

static void *producer(void *arg)
{
    long first_id = (long)arg * ITEMS_PER_PRODUCER; /* ids are unique per producer */
    for (long i = 0; i < ITEMS_PER_PRODUCER; i++)
        put_item(first_id + i);
    return NULL;
}

static void *consumer(void *arg)
{
    for (;;) {
        long item = take_item();
        if (item == STOP_ITEM)
            break;
        __sync_fetch_and_add(&seen[item], 1);
    }
    return NULL;
}

int main(void)
{
    pthread_t producers[NUM_PRODUCERS], consumers[NUM_CONSUMERS];

    sem_init(&empty_slots, 0, BUFFER_SIZE);
    sem_init(&full_slots, 0, 0);

    double wall_start = now_ms(CLOCK_MONOTONIC);
    double cpu_start = now_ms(CLOCK_PROCESS_CPUTIME_ID);

    for (long i = 0; i < NUM_CONSUMERS; i++)
        pthread_create(&consumers[i], NULL, consumer, NULL);
    for (long i = 0; i < NUM_PRODUCERS; i++)
        pthread_create(&producers[i], NULL, producer, (void *)i);
    for (int i = 0; i < NUM_PRODUCERS; i++)
        pthread_join(producers[i], NULL);
    for (int i = 0; i < NUM_CONSUMERS; i++)
        put_item(STOP_ITEM); /* one stop item per consumer */
    for (int i = 0; i < NUM_CONSUMERS; i++)
        pthread_join(consumers[i], NULL);

    double wall = now_ms(CLOCK_MONOTONIC) - wall_start;
    double cpu = now_ms(CLOCK_PROCESS_CPUTIME_ID) - cpu_start;

    long lost = 0, duplicated = 0;
    for (long id = 0; id < TOTAL_ITEMS; id++) {
        if (seen[id] == 0)
            lost++;
        else if (seen[id] > 1)
            duplicated++;
    }

    printf("[sem P=%d C=%d buffer=%d] items=%d lost=%ld duplicated=%ld "
           "time=%.2f ms cpu=%.2f ms\n",
           NUM_PRODUCERS, NUM_CONSUMERS, BUFFER_SIZE, TOTAL_ITEMS,
           lost, duplicated, wall, cpu);

    sem_destroy(&empty_slots);
    sem_destroy(&full_slots);
    return 0;
}
