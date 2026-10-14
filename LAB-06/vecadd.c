/* Lab 06, Task 2: Vector addition with #pragma omp parallel for.
 *
 * c[i] = a[i] + b[i] for i = 0 .. N-1, repeated PASSES times so the timing is
 * long enough to measure. The loop body is pure element-wise work: every
 * iteration touches its own index only, so no lock or atomic is needed and the
 * result is always correct.
 *
 * The scoping experiment for Question 2 is a compile switch: build with
 * -DDROP_N_FROM_SHARED and the compiler rejects the program because default(none)
 * leaves n unscoped.
 *
 * Build: gcc -O2 -fopenmp -std=gnu11 vecadd.c -o vecadd
 * Run  : OMP_NUM_THREADS=4 ./vecadd
 */
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#ifndef N
#define N 5000000          /* 5,000,000 elements */
#endif
#ifndef PASSES
#define PASSES 20          /* 20 passes per timed run */
#endif

static void vec_add(const double *a, const double *b, double *c, long n)
{
    long i;
#ifdef DROP_N_FROM_SHARED
    /* Question 2 experiment: n is missing from the shared(...) list. With
     * default(none) the compiler must refuse to guess its data-sharing. */
    #pragma omp parallel for default(none) shared(a, b, c) private(i)
#else
    #pragma omp parallel for default(none) shared(a, b, c, n) private(i)
#endif
    for (i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}

int main(void)
{
    double *a = malloc(N * sizeof *a);
    double *b = malloc(N * sizeof *b);
    double *c = malloc(N * sizeof *c);
    if (!a || !b || !c) { fprintf(stderr, "out of memory\n"); return 1; }

    for (long i = 0; i < N; i++) { a[i] = (double)i; b[i] = 2.0 * (double)i; }

    double t0 = omp_get_wtime();
    for (int p = 0; p < PASSES; p++)
        vec_add(a, b, c, N);
    double t1 = omp_get_wtime();

    /* Verify: every c[i] must equal 3*i. */
    long wrong = 0;
    for (long i = 0; i < N; i++)
        if (c[i] != 3.0 * (double)i) wrong++;

    double ms = (t1 - t0) * 1000.0;
    printf("threads=%d  N=%d  passes=%d  time=%.2f ms  correct=%s\n",
           omp_get_max_threads(), N, PASSES, ms, wrong == 0 ? "YES" : "NO");

    free(a); free(b); free(c);
    return 0;
}
