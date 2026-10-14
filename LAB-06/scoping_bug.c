/* Lab 06, Task 3: Missing private bug and its fixes.
 * Each iteration: tmp = in[i]*3;  out[i] = tmp + 7;  (correct = 3*in[i]+7)
 *
 * Buggy : gcc -O0 -fopenmp -std=gnu11 scoping_bug.c -o bug
 * Fix 1 : gcc -O0 -fopenmp -std=gnu11 -DFIX_PRIVATE scoping_bug.c -o fix_private
 * Fix 2 : gcc -O0 -fopenmp -std=gnu11 -DFIX_LOCAL   scoping_bug.c -o fix_local
 * Error : gcc -O0 -fopenmp -std=gnu11 -DDEFAULT_NONE scoping_bug.c -o bug_none
 * Run   : OMP_NUM_THREADS=4 ./bug
 */
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define N 1000000

int main(void)
{
    int *in  = malloc(N * sizeof(int));
    int *out = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) in[i] = i % 1000;

    int i, tmp;

#if defined(FIX_LOCAL)
    #pragma omp parallel for private(i)              /* tmp declared inside loop */
    for (i = 0; i < N; i++) {
        int tmp = in[i] * 3;
        out[i] = tmp + 7;
    }
#elif defined(FIX_PRIVATE)
    #pragma omp parallel for private(i, tmp)         /* each thread gets own tmp */
    for (i = 0; i < N; i++) {
        tmp = in[i] * 3;
        out[i] = tmp + 7;
    }
#elif defined(DEFAULT_NONE)
    #pragma omp parallel for default(none) shared(in, out) private(i)  /* tmp unscoped -> error */
    for (i = 0; i < N; i++) {
        tmp = in[i] * 3;
        out[i] = tmp + 7;
    }
#else
    #pragma omp parallel for private(i)              /* BUG: tmp is shared */
    for (i = 0; i < N; i++) {
        tmp = in[i] * 3;
        out[i] = tmp + 7;
    }
#endif

    int wrong = 0;
    for (i = 0; i < N; i++)
        if (out[i] != in[i] * 3 + 7) wrong++;

    printf("threads=%d  N=%d  wrong_elements=%d\n", omp_get_max_threads(), N, wrong);

    free(in); free(out);
    return 0;
}
