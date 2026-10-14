/* Lab 06, Task 1: Parallel region and thread IDs.
 * Build: gcc -O2 -fopenmp -std=gnu11 hello_omp.c -o hello_omp
 * Clause build: add -DCLAUSE_THREADS=3
 * Run:   OMP_NUM_THREADS=4 ./hello_omp
 */
#include <stdio.h>
#include <omp.h>

int main(void)
{
    printf("before region: num_threads = %d\n", omp_get_num_threads());

#ifdef CLAUSE_THREADS
    #pragma omp parallel num_threads(CLAUSE_THREADS)
#else
    #pragma omp parallel
#endif
    {
        int id   = omp_get_thread_num();
        int size = omp_get_num_threads();
        printf("Hello from thread %d of %d\n", id, size);
    }

    printf("after region:  num_threads = %d\n", omp_get_num_threads());
    return 0;
}
