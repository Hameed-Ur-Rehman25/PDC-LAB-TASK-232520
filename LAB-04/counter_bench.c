/*
 * Lab 04 - Task 4: Atomic increment vs. critical section
 * The increment method is chosen at compile time:
 *   -DUSE_MUTEX  pthread mutex around counter++ (Task 3)
 *   -DUSE_SYNC   GCC __sync_fetch_and_add
 *   -DUSE_C11    C11 atomic_fetch_add
 */
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#ifndef NUM_THREADS
#define NUM_THREADS 4
#endif
#ifndef INCREMENTS_PER_THREAD
#define INCREMENTS_PER_THREAD 1000000
#endif

#if defined(USE_MUTEX)
  static long counter = 0;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  #define INCREMENT() do { pthread_mutex_lock(&lock); counter++; \
                           pthread_mutex_unlock(&lock); } while (0)
  #define READ_COUNTER() counter
  #define VERSION "mutex"
#elif defined(USE_SYNC)
  static long counter = 0;
  #define INCREMENT() __sync_fetch_and_add(&counter, 1)
  #define READ_COUNTER() counter
  #define VERSION "__sync_fetch_and_add"
#elif defined(USE_C11)
  static atomic_long counter = 0;
  #define INCREMENT() atomic_fetch_add(&counter, 1)
  #define READ_COUNTER() atomic_load(&counter)
  #define VERSION "C11 atomic"
#else
  #error "Compile with -DUSE_MUTEX, -DUSE_SYNC or -DUSE_C11"
#endif

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void *worker(void *arg)
{
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++)
        INCREMENT();
    return NULL;
}

int main(void)
{
    pthread_t threads[NUM_THREADS];
    double start = now_ms();

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, NULL);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    double elapsed = now_ms() - start;
    long expected = (long)NUM_THREADS * INCREMENTS_PER_THREAD;
    long actual = READ_COUNTER();
    printf("[%s] expected=%ld actual=%ld correct=%s time=%.2f ms\n",
           VERSION, expected, actual,
           expected == actual ? "YES" : "NO", elapsed);
    return 0;
}
