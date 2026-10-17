/* reduction.c - Task 1: parallel sum and max, reduction clause vs critical sections
 * Build: gcc -O2 -fopenmp -std=gnu11 reduction.c -o reduction
 * Run:   OMP_NUM_THREADS=4 ./reduction
 */
#include <stdio.h>
#include <limits.h>
#include <omp.h>

#ifndef N
#define N 10000000
#endif
#ifndef REPS
#define REPS 3
#endif

static int data[N];

static void serial_version(long long *sum, int *mx)
{
    long long s = 0;
    int m = INT_MIN;
    for (int i = 0; i < N; i++) {
        s += data[i];
        if (data[i] > m) m = data[i];
    }
    *sum = s; *mx = m;
}

static void racy_version(long long *sum, int *mx)       /* wrong on purpose */
{
    long long s = 0;
    int m = INT_MIN;
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        s += data[i];
        if (data[i] > m) m = data[i];
    }
    *sum = s; *mx = m;
}

static void critical_each_version(long long *sum, int *mx)
{
    long long s = 0;
    int m = INT_MIN;
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        /* Protect every update of s and m: only one thread at a time runs this. */
        #pragma omp critical
        {
            s += data[i];
            if (data[i] > m) m = data[i];
        }
    }
    *sum = s; *mx = m;
}

static void critical_partial_version(long long *sum, int *mx)
{
    long long s = 0;
    int m = INT_MIN;
    #pragma omp parallel
    {
        long long local_sum = 0;                        /* private partial results */
        int local_max = INT_MIN;
        #pragma omp for nowait
        for (int i = 0; i < N; i++) {
            local_sum += data[i];
            if (data[i] > local_max) local_max = data[i];
        }
        #pragma omp critical                    /* one short critical section per thread */
        {
            s += local_sum;
            if (local_max > m) m = local_max;
        }
    }
    *sum = s; *mx = m;
}

static void reduction_version(long long *sum, int *mx)
{
    long long s = 0;
    int m = INT_MIN;
    /* Each thread gets private copies of s and m (starting at the identity values
       0 and INT_MIN), which the runtime combines at the end of the loop. */
    #pragma omp parallel for reduction(+:s) reduction(max:m)
    for (int i = 0; i < N; i++) {
        s += data[i];
        if (data[i] > m) m = data[i];
    }
    *sum = s; *mx = m;
}

typedef void (*variant_fn)(long long *, int *);

int main(void)
{
    static const char *names[] = { "serial", "racy", "critical-each",
                                   "critical-partial", "reduction" };
    variant_fn fns[] = { serial_version, racy_version, critical_each_version,
                         critical_partial_version, reduction_version };

    for (int i = 0; i < N; i++)
        data[i] = (int)(((long long)i * 7919) % 100003);

    long long ref_sum; int ref_max;
    serial_version(&ref_sum, &ref_max);                 /* reference result */

    printf("threads=%d n=%d reps=%d\n", omp_get_max_threads(), N, REPS);
    for (int v = 0; v < 5; v++) {
        long long sum = 0; int mx = 0;
        double total = 0.0;
        for (int r = 0; r < REPS; r++) {
            double t0 = omp_get_wtime();
            fns[v](&sum, &mx);
            total += omp_get_wtime() - t0;
        }
        printf("[%-16s] sum=%lld max=%d sum_ok=%s max_ok=%s avg_time=%.2f ms\n",
               names[v], sum, mx, sum == ref_sum ? "YES" : "NO",
               mx == ref_max ? "YES" : "NO", total / REPS * 1000.0);
    }
    return 0;
}
