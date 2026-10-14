/* Lab 06, Task 4: Independent operations with sections.
 * A = sum of sqrt(i), B = sum of sin*cos, C = sum of log(i+1).
 * Build: gcc -O2 -fopenmp -std=gnu11 omp_sections.c -o omp_sections -lm
 * Run:   OMP_NUM_THREADS=3 ./omp_sections
 */
#include <stdio.h>
#include <math.h>
#include <omp.h>

#define WORK 120000000L

double op_A(void) { double s = 0; for (long i = 0; i < WORK; i++) s += sqrt(i);            return s; }
double op_B(void) { double s = 0; for (long i = 0; i < WORK; i++) s += sin(i) * cos(i);     return s; }
double op_C(void) { double s = 0; for (long i = 0; i < WORK; i++) s += log(i + 1.0);        return s; }

int main(void)
{
    double t, tA, tB, tC;
    double a_ref, b_ref, c_ref;

    /* serial reference */
    t = omp_get_wtime(); a_ref = op_A(); tA = omp_get_wtime() - t;
    t = omp_get_wtime(); b_ref = op_B(); tB = omp_get_wtime() - t;
    t = omp_get_wtime(); c_ref = op_C(); tC = omp_get_wtime() - t;
    printf("serial: A=%.3fs B=%.3fs C=%.3fs total=%.3fs\n", tA, tB, tC, tA + tB + tC);

    /* parallel sections */
    double a = 0, b = 0, c = 0;
    int thrA = -1, thrB = -1, thrC = -1;

    double wall = omp_get_wtime();
    #pragma omp parallel sections
    {
        #pragma omp section
        { thrA = omp_get_thread_num(); a = op_A(); }
        #pragma omp section
        { thrB = omp_get_thread_num(); b = op_B(); }
        #pragma omp section
        { thrC = omp_get_thread_num(); c = op_C(); }
    }
    wall = omp_get_wtime() - wall;

    int match = (a == a_ref && b == b_ref && c == c_ref);
    printf("threads=%d  wall=%.3fs  speedup=%.2f  thread_of_A=%d B=%d C=%d  results_match=%s\n",
           omp_get_max_threads(), wall, (tA + tB + tC) / wall,
           thrA, thrB, thrC, match ? "YES" : "NO");
    return 0;
}
