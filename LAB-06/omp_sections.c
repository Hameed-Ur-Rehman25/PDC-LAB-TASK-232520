/* Lab 06, Task 4: Independent operations with #pragma omp sections.
 *
 * Three independent operations run concurrently, one per section:
 *   A: sum of sqrt(i)          over a large range
 *   B: sum of sin(i)*cos(i)    over a large range  (the heaviest)
 *   C: sum of log(i+1)         over a large range
 * Each writes only its own result variable, so the sections are independent and
 * need no synchronization. A thread that picks up a section prints which section
 * it ran, so we can see the assignment the runtime chose.
 *
 * The program first times A, B and C one after another (serial reference), then
 * times them again inside a sections construct, and checks the two sets of
 * results match.
 *
 * Build: gcc -O2 -fopenmp -std=gnu11 omp_sections.c -o omp_sections
 * Run  : OMP_NUM_THREADS=3 ./omp_sections
 */
#include <stdio.h>
#include <math.h>
#include <omp.h>

#ifndef WORK
#define WORK 120000000L     /* iterations per section */
#endif

static double op_A(void) { double s = 0; for (long i = 0; i < WORK; i++) s += sqrt((double)i);              return s; }
static double op_B(void) { double s = 0; for (long i = 0; i < WORK; i++) s += sin((double)i)*cos((double)i); return s; }
static double op_C(void) { double s = 0; for (long i = 0; i < WORK; i++) s += log((double)i + 1.0);          return s; }

int main(void)
{
    double a_ref, b_ref, c_ref;
    double tA, tB, tC, t;

    /* ---- Serial reference: time each operation on its own ---- */
    t = omp_get_wtime(); a_ref = op_A(); tA = omp_get_wtime() - t;
    t = omp_get_wtime(); b_ref = op_B(); tB = omp_get_wtime() - t;
    t = omp_get_wtime(); c_ref = op_C(); tC = omp_get_wtime() - t;
    double serial_total = tA + tB + tC;

    printf("serial: A=%.3fs B=%.3fs C=%.3fs total=%.3fs\n", tA, tB, tC, serial_total);

    /* ---- Parallel: the three operations as independent sections ---- */
    double a_par = 0, b_par = 0, c_par = 0;
    int thrA = -1, thrB = -1, thrC = -1;

    double wall0 = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp sections
        {
            #pragma omp section
            { thrA = omp_get_thread_num(); a_par = op_A(); }

            #pragma omp section
            { thrB = omp_get_thread_num(); b_par = op_B(); }

            #pragma omp section
            { thrC = omp_get_thread_num(); c_par = op_C(); }
        }   /* implicit barrier: no thread leaves until A, B and C are all done */
    }
    double wall = omp_get_wtime() - wall0;

    int match = (a_par == a_ref) && (b_par == b_ref) && (c_par == c_ref);

    printf("threads=%d  wall=%.3fs  speedup=%.2f  thread_of_A=%d B=%d C=%d  results_match=%s\n",
           omp_get_max_threads(), wall, serial_total / wall,
           thrA, thrB, thrC, match ? "YES" : "NO");

    return 0;
}
