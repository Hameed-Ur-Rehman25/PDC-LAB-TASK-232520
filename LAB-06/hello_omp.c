/* Lab 06, Task 1: Parallel region and thread IDs.
 *
 * Each thread in the team prints its own ID and the team size. The size of the
 * team is normally taken from OMP_NUM_THREADS, but building with
 * -DCLAUSE_THREADS=n adds a num_threads(n) clause so we can see which setting
 * wins when the two disagree (Question 3).
 *
 * Build : gcc -O2 -fopenmp -std=gnu11 hello_omp.c -o hello_omp
 * Clause: gcc -O2 -fopenmp -std=gnu11 -DCLAUSE_THREADS=3 hello_omp.c -o hello_clause
 * Run   : OMP_NUM_THREADS=4 ./hello_omp
 */
#include <stdio.h>
#include <omp.h>

int main(void)
{
    /* omp_get_num_threads() outside any parallel region always reports 1:
     * the program is running on the single initial (master) thread here. */
    printf("before region: omp_get_num_threads() = %d\n", omp_get_num_threads());

#ifdef CLAUSE_THREADS
    #pragma omp parallel num_threads(CLAUSE_THREADS)
#else
    #pragma omp parallel
#endif
    {
        int id   = omp_get_thread_num();   /* 0 .. team_size-1, private to each thread */
        int size = omp_get_num_threads();  /* the actual team size inside the region   */

        /* One printf per call keeps each line atomic, but the OpenMP standard
         * gives no ordering guarantee, so the lines may arrive in any order. */
        printf("Hello from thread %d of %d\n", id, size);
    }

    printf("after region:  omp_get_num_threads() = %d\n", omp_get_num_threads());
    return 0;
}
