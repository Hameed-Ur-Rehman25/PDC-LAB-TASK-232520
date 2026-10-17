/* false_sharing.c - Task 3: adjacent per-thread counters, padding, local accumulation
 * Build: gcc -O2 -fopenmp -std=gnu11 false_sharing.c -o false_sharing
 * Run:   OMP_NUM_THREADS=4 ./false_sharing
 */
#include <stdio.h>
#include <omp.h>

#ifndef ITERS
#define ITERS 50000000L
#endif
#define MAX_THREADS 64
#define CACHE_LINE 64

static volatile long adjacent[MAX_THREADS];     /* 8 bytes apart: 8 per cache line */

typedef struct {
    volatile long value;
    char pad[CACHE_LINE - sizeof(long)];        /* fills the rest of the cache line */
} padded_t;
static padded_t padded[MAX_THREADS] __attribute__((aligned(CACHE_LINE)));

static long local_result[MAX_THREADS];                  /* written once per thread */

static double run(int variant, int team)
{
    double t0 = omp_get_wtime();
    #pragma omp parallel num_threads(team)
    {
        int id = omp_get_thread_num();
        if (variant == 0) {
            /* Increment this thread's own adjacent counter ITERS times. The counters
               are 8 bytes apart, so several share one cache line: false sharing. */
            for (long i = 0; i < ITERS; i++)
                adjacent[id]++;
        } else if (variant == 1) {
            /* Same work on the padded counter: each sits on its own cache line. */
            for (long i = 0; i < ITERS; i++)
                padded[id].value++;
        } else {
            long local = 0;                             /* private to the thread */
            for (long i = 0; i < ITERS; i++) {
                local++;
                __asm__ volatile ("" : "+r"(local));    /* keeps the loop from folding */
            }
            local_result[id] = local;                   /* one write at the end */
        }
    }
    return (omp_get_wtime() - t0) * 1000.0;
}

int main(void)
{
    omp_set_dynamic(0);
    int team = omp_get_max_threads();
    if (team > MAX_THREADS) team = MAX_THREADS;

    printf("threads=%d iterations=%ld per thread\n", team, ITERS);
    printf("adjacent counters are %ld bytes apart; padded counters are %ld bytes apart\n",
           (long)((char *)&adjacent[1] - (char *)&adjacent[0]),
           (long)((char *)&padded[1] - (char *)&padded[0]));

    static const char *names[] = { "adjacent", "padded", "local-accumulate" };
    double ms[3];
    for (int v = 0; v < 3; v++) ms[v] = run(v, team);

    int ok = 1;
    for (int t = 0; t < team; t++)
        if (adjacent[t] != ITERS || padded[t].value != ITERS ||
            local_result[t] != ITERS) ok = 0;

    for (int v = 0; v < 3; v++)
        printf("[%-16s] time=%8.2f ms  slowdown vs padded=%5.2fx\n",
               names[v], ms[v], ms[v] / ms[1]);
    printf("counters correct=%s\n", ok ? "YES" : "NO");
    return 0;
}
