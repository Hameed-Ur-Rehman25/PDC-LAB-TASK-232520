/*
 * Lab 05 - Task 4: Reusable phase barrier
 * NUM_WORKERS threads update a ring of cells for ROUNDS rounds. Every round
 * has two phases, each ended by a barrier:
 *   phase 1: compute next[] for the own slice from cur[] (reads neighbours)
 *   phase 2: copy the own slice of next[] back into cur[]
 * The result is compared cell by cell with a single-threaded reference.
 *   -DNO_BARRIER  experiment: barrier_wait() does nothing
 */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#ifndef NUM_WORKERS
#define NUM_WORKERS 4
#endif
#ifndef ROUNDS
#define ROUNDS 10000
#endif
#ifndef CELLS
#define CELLS 4096
#endif

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t all_arrived;
    int parties;         /* threads needed to release the barrier */
    int waiting;         /* threads that have arrived in this generation */
    long generation;     /* incremented every time the barrier opens */
} barrier_t;

static barrier_t phase_barrier;
static uint32_t cur[CELLS], next[CELLS]; /* shared grid */
static uint32_t ref_cur[CELLS], ref_next[CELLS]; /* sequential reference */

static void barrier_init(barrier_t *b, int parties)
{
    pthread_mutex_init(&b->lock, NULL);
    pthread_cond_init(&b->all_arrived, NULL);
    b->parties = parties;
    b->waiting = 0;
    b->generation = 0;
}

static void barrier_wait(barrier_t *b)
{
#ifndef NO_BARRIER
    pthread_mutex_lock(&b->lock);
    long my_generation = b->generation;
    if (++b->waiting == b->parties) {
        /* last arrival: open the barrier and reset it for the next use */
        b->generation++;
        b->waiting = 0;
        pthread_cond_broadcast(&b->all_arrived);
    } else {
        while (my_generation == b->generation)
            pthread_cond_wait(&b->all_arrived, &b->lock);
    }
    pthread_mutex_unlock(&b->lock);
#endif
}

static uint32_t next_value(const uint32_t *grid, int i, int round)
{
    int left = (i + CELLS - 1) % CELLS;
    int right = (i + 1) % CELLS;
    return grid[left] * 3u + grid[i] * 5u + grid[right] * 7u + (uint32_t)round;
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void *worker(void *arg)
{
    long id = (long)arg;
    int first = id * CELLS / NUM_WORKERS;
    int last = (id + 1) * CELLS / NUM_WORKERS;

    for (int round = 0; round < ROUNDS; round++) {
        for (int i = first; i < last; i++)
            next[i] = next_value(cur, i, round);
        barrier_wait(&phase_barrier); /* everyone has finished reading cur[] */

        for (int i = first; i < last; i++)
            cur[i] = next[i];
        barrier_wait(&phase_barrier); /* everyone has finished writing cur[] */
    }
    return NULL;
}

int main(void)
{
    pthread_t workers[NUM_WORKERS];

    for (int i = 0; i < CELLS; i++)
        cur[i] = ref_cur[i] = (uint32_t)i * 2654435761u;

    for (int round = 0; round < ROUNDS; round++) {
        for (int i = 0; i < CELLS; i++)
            ref_next[i] = next_value(ref_cur, i, round);
        for (int i = 0; i < CELLS; i++)
            ref_cur[i] = ref_next[i];
    }

    barrier_init(&phase_barrier, NUM_WORKERS);
    double start = now_ms();
    for (long i = 0; i < NUM_WORKERS; i++)
        pthread_create(&workers[i], NULL, worker, (void *)i);
    for (int i = 0; i < NUM_WORKERS; i++)
        pthread_join(workers[i], NULL);
    double elapsed = now_ms() - start;

    int mismatches = 0;
    uint32_t checksum = 0;
    for (int i = 0; i < CELLS; i++) {
        if (cur[i] != ref_cur[i])
            mismatches++;
        checksum ^= cur[i];
    }

#ifdef NO_BARRIER
    const char *version = "no barrier";
#else
    const char *version = "barrier";
#endif
    printf("[%s workers=%d rounds=%d] correct=%s mismatching_cells=%d/%d "
           "checksum=%08x time=%.2f ms\n", version, NUM_WORKERS, ROUNDS,
           mismatches == 0 ? "YES" : "NO", mismatches, CELLS, checksum, elapsed);
    return 0;
}
