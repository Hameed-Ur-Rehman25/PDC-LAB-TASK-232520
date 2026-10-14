/* Lab 06, Task 3: A data-scoping bug (missing private) and its fixes.
 *
 * Each iteration computes   tmp = in[i] * 3;  out[i] = tmp + 7;
 * so the correct result is out[i] = 3*in[i] + 7.
 *
 * In the buggy build tmp is declared OUTSIDE the parallel region and never
 * scoped, so all threads share the one tmp. Thread X can overwrite tmp between
 * another thread's two statements, and out[i] comes out wrong. The program
 * counts how many elements are wrong so a "correct-looking" run cannot hide the
 * race.
 *
 * Four builds, selected at compile time:
 *   (default)        buggy: shared tmp, the race is present
 *   -DFIX_PRIVATE    fix 1: add private(tmp) so each thread gets its own copy
 *   -DFIX_LOCAL      fix 2: declare tmp inside the loop body (automatically private)
 *   -DDEFAULT_NONE   add default(none) without scoping tmp -> compile error
 *
 * Buggy build: gcc -O0 -fopenmp -std=gnu11 scoping_bug.c -o bug
 * Fix 1      : gcc -O0 -fopenmp -std=gnu11 -DFIX_PRIVATE scoping_bug.c -o fix_private
 * Fix 2      : gcc -O0 -fopenmp -std=gnu11 -DFIX_LOCAL   scoping_bug.c -o fix_local
 * Error demo : gcc -O0 -fopenmp -std=gnu11 -DDEFAULT_NONE scoping_bug.c -o bug_none
 */
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#ifndef N
#define N 1000000
#endif

int main(void)
{
    int *in  = malloc(N * sizeof *in);
    int *out = malloc(N * sizeof *out);
    if (!in || !out) { fprintf(stderr, "out of memory\n"); return 1; }
    for (long i = 0; i < N; i++) in[i] = (int)(i % 1000);

    long i;

#if defined(FIX_LOCAL)
    /* Fix 2: tmp is local to each iteration, so it is private by construction. */
    #pragma omp parallel for shared(in, out) private(i)
    for (i = 0; i < N; i++) {
        int tmp = in[i] * 3;
        out[i] = tmp + 7;
    }

#elif defined(DEFAULT_NONE)
    /* default(none) forces every variable to be scoped. tmp is left out on
     * purpose, so this build must fail to compile (captured for the report). */
    int tmp;
    #pragma omp parallel for default(none) shared(in, out) private(i)
    for (i = 0; i < N; i++) {
        tmp = in[i] * 3;
        out[i] = tmp + 7;
    }

#elif defined(FIX_PRIVATE)
    /* Fix 1: the shared-looking tmp is given a private copy per thread. */
    int tmp;
    #pragma omp parallel for shared(in, out) private(i, tmp)
    for (i = 0; i < N; i++) {
        tmp = in[i] * 3;
        out[i] = tmp + 7;
    }

#else
    /* Buggy version: one tmp shared by the whole team -> race condition. */
    int tmp;
    #pragma omp parallel for shared(in, out, tmp) private(i)
    for (i = 0; i < N; i++) {
        tmp = in[i] * 3;
        out[i] = tmp + 7;
    }
#endif

    long wrong = 0;
    for (long k = 0; k < N; k++)
        if (out[k] != in[k] * 3 + 7) wrong++;

    printf("threads=%d  N=%d  wrong_elements=%ld\n",
           omp_get_max_threads(), N, wrong);

    free(in); free(out);
    return 0;
}
