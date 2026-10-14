/*
 * Lab 05 - Task 3: Condition-variable signaling
 * Workers block until the "data ready" flag is set; publish() sets the flag
 * and wakes them up. Variants chosen at compile time:
 *   (default)       workers wait first, main publishes after DELAY_MS
 *   -DSIGNAL_FIRST  main publishes before the workers start
 *   -DUSE_SPIN      workers poll the flag in a busy loop instead of blocking
 *   -DNO_SIGNAL     lost-signal experiment: publish() sets the flag only
 *   -DSIGNAL_ONE    publish() uses pthread_cond_signal instead of broadcast
 */
#include <stdio.h>
#include <pthread.h>
#include <time.h>

#ifndef NUM_WORKERS
#define NUM_WORKERS 1
#endif
#ifndef DELAY_MS
#define DELAY_MS 1000
#endif

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;
static int data_ready = 0;    /* the predicate, guarded by lock */
static int shared_value = 0;  /* the data, guarded by lock */

static long times_blocked[NUM_WORKERS]; /* calls to pthread_cond_wait */
static long times_polled[NUM_WORKERS];  /* failed checks in the spin variant */
static int received[NUM_WORKERS];

static double now_ms(clockid_t clock)
{
    struct timespec ts;
    clock_gettime(clock, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void *worker(void *arg)
{
    long id = (long)arg;

#ifdef USE_SPIN
    for (;;) {
        pthread_mutex_lock(&lock);
        if (data_ready)
            break; /* leave the loop with the lock held */
        pthread_mutex_unlock(&lock);
        times_polled[id]++;
    }
#else
    pthread_mutex_lock(&lock);
    while (!data_ready) { /* re-check the predicate after every wake-up */
        times_blocked[id]++;
        pthread_cond_wait(&ready_cond, &lock);
    }
#endif
    received[id] = shared_value;
    pthread_mutex_unlock(&lock);
    return NULL;
}

static void publish(int value)
{
    pthread_mutex_lock(&lock);
    shared_value = value;
    data_ready = 1;
#if defined(NO_SIGNAL)
    /* nobody is woken up */
#elif defined(SIGNAL_ONE)
    pthread_cond_signal(&ready_cond);
#else
    pthread_cond_broadcast(&ready_cond); /* wake every waiting worker */
#endif
    pthread_mutex_unlock(&lock);
}

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

int main(void)
{
    pthread_t workers[NUM_WORKERS];
    double wall_start = now_ms(CLOCK_MONOTONIC);
    double cpu_start = now_ms(CLOCK_PROCESS_CPUTIME_ID);

#ifdef SIGNAL_FIRST
    publish(42);
#endif
    for (long i = 0; i < NUM_WORKERS; i++)
        pthread_create(&workers[i], NULL, worker, (void *)i);
#ifndef SIGNAL_FIRST
    sleep_ms(DELAY_MS); /* the workers are waiting during this time */
    publish(42);
#endif
    for (int i = 0; i < NUM_WORKERS; i++)
        pthread_join(workers[i], NULL);

    double wall = now_ms(CLOCK_MONOTONIC) - wall_start;
    double cpu = now_ms(CLOCK_PROCESS_CPUTIME_ID) - cpu_start;

    for (int i = 0; i < NUM_WORKERS; i++)
        printf("worker %d: blocked=%ld polls=%ld value=%d\n",
               i, times_blocked[i], times_polled[i], received[i]);
    printf("wall=%.2f ms cpu=%.2f ms\n", wall, cpu);
    return 0;
}
