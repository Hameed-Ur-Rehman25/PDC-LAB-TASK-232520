/* schedule_bench.c - Task 2: static, dynamic and guided scheduling on an uneven workload
 * Build:         gcc -O2 -fopenmp -std=gnu11 schedule_bench.c -o schedule_bench
 * Uniform build: gcc -O2 -fopenmp -std=gnu11 -DUNIFORM schedule_bench.c -o schedule_bench_uniform
 * Run:           OMP_NUM_THREADS=4 ./schedule_bench
 */
#include <stdio.h>
#include <string.h>
#include <omp.h>

#ifndef N
#define N 20000
#endif
#ifndef REPS
#define REPS 3
#endif
#define MAX_THREADS 64

static long work(int i)
{
#ifdef UNIFORM
    int steps = N / 2;                      /* every iteration costs the same */
#else
    int steps = i;                          /* iteration i costs i steps: uneven */
#endif
    long acc = i;
    for (int j = 0; j < steps; j++)
        acc = acc * 31 + (j ^ i);
    return acc;
}

typedef struct { const char *name; omp_sched_t kind; int chunk; } policy_t;

static const policy_t policies[] = {
    { "static",     omp_sched_static,  0  },
    { "static,1",   omp_sched_static,  1  },
    { "static,64",  omp_sched_static,  64 },
    { "dynamic,1",  omp_sched_dynamic, 1  },
    { "dynamic,64", omp_sched_dynamic, 64 },
    { "guided,1",   omp_sched_guided,  1  },
    { "guided,64",  omp_sched_guided,  64 }
};
#define NUM_POLICIES (int)(sizeof(policies) / sizeof(policies[0]))

static long result[N], expected[N];

static void run_policy(const policy_t *p, double *wall, double *bmax, double *bmin)
{
    double busy[MAX_THREADS] = { 0 };
    int team = omp_get_max_threads();
    omp_set_schedule(p->kind, p->chunk);
    double t0 = omp_get_wtime();
    #pragma omp parallel
    {
        double t = omp_get_wtime();
        /* Share the loop among the threads with the runtime schedule; nowait lets
           every thread stop its own clock as soon as its share is finished. */
        #pragma omp for schedule(runtime) nowait
        for (int i = 0; i < N; i++)
            result[i] = work(i);
        busy[omp_get_thread_num()] = omp_get_wtime() - t;
    }
    *wall = omp_get_wtime() - t0;
    *bmax = busy[0]; *bmin = busy[0];
    for (int t = 1; t < team; t++) {
        if (busy[t] > *bmax) *bmax = busy[t];
        if (busy[t] < *bmin) *bmin = busy[t];
    }
}

int main(void)
{
    omp_set_dynamic(0);                     /* use exactly the requested threads */
    if (omp_get_max_threads() > MAX_THREADS) {
        fprintf(stderr, "too many threads\n");
        return 1;
    }

    double s0 = omp_get_wtime();
    for (int i = 0; i < N; i++) expected[i] = work(i);
    double serial_ms = (omp_get_wtime() - s0) * 1000.0;

#ifdef UNIFORM
    printf("workload=uniform");
#else
    printf("workload=uneven");
#endif
    printf(" n=%d threads=%d serial=%.1f (all times in ms)\n",
           N, omp_get_max_threads(), serial_ms);

    for (int k = 0; k < NUM_POLICIES; k++) {
        double best = 1e30, bmax = 0, bmin = 0;
        int ok = 1;
        for (int r = 0; r < REPS; r++) {
            double w, mx, mn;
            memset(result, 0, sizeof result);
            run_policy(&policies[k], &w, &mx, &mn);
            if (memcmp(result, expected, sizeof result) != 0) ok = 0;
            if (w < best) { best = w; bmax = mx; bmin = mn; }
        }
        printf("[%-10s] wall=%7.1f busy_max=%7.1f busy_min=%7.1f "
               "imbalance=%5.2f correct=%s\n",
               policies[k].name, best * 1000.0, bmax * 1000.0, bmin * 1000.0,
               bmin > 1e-9 ? bmax / bmin : 0.0, ok ? "YES" : "NO");
    }
    return 0;
}
