/* Lab 06, Task 2: Vector addition with parallel for.
 * Build: gcc -O2 -fopenmp -std=gnu11 vecadd.c -o vecadd
 * Run:   OMP_NUM_THREADS=4 ./vecadd
 *
 * Scoping experiment (Q2): remove n from the shared(...) list below and rebuild;
 * default(none) makes the compiler reject it. Then put n back.
 */
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define N 5000000
#define PASSES 20

void vec_add(double *a, double *b, double *c, int n)
{
    int i;
    #pragma omp parallel for default(none) shared(a, b, c, n) private(i)
    for (i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}

int main(void)
{
    double *a = malloc(N * sizeof(double));
    double *b = malloc(N * sizeof(double));
    double *c = malloc(N * sizeof(double));

    for (int i = 0; i < N; i++) { a[i] = i; b[i] = 2 * i; }

    double t0 = omp_get_wtime();
    for (int p = 0; p < PASSES; p++)
        vec_add(a, b, c, N);
    double t1 = omp_get_wtime();

    int correct = 1;
    for (int i = 0; i < N; i++)
        if (c[i] != 3.0 * i) { correct = 0; break; }

    printf("threads=%d  N=%d  passes=%d  time=%.2f ms  correct=%s\n",
           omp_get_max_threads(), N, PASSES, (t1 - t0) * 1000.0,
           correct ? "YES" : "NO");

    free(a); free(b); free(c);
    return 0;
}
