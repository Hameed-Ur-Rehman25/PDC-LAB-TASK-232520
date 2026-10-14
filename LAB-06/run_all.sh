#!/bin/bash
# Runs every Lab 06 task in order. Must be run on Linux (WSL or a container) with
# GCC and OpenMP (libgomp). Binaries are built in /tmp/lab06 so the source folder
# stays clean.
set -u
SRC="$(cd "$(dirname "$0")" && pwd)"
BUILD=/tmp/lab06
mkdir -p "$BUILD" && cd "$BUILD"

CC="gcc -O2 -fopenmp -std=gnu11"
CC0="gcc -O0 -fopenmp -std=gnu11"   # -O0 for the Task 3 race, as the sheet requires

echo "===== Toolchain ====="
gcc --version | head -1
echo "nproc: $(nproc)"
echo 'int main(void){return 0;}' > omp_test.c
gcc -fopenmp omp_test.c -o omp_test && ./omp_test && echo "OpenMP OK"

echo; echo "============================================================"
echo "===== Task 1: hello_omp.c -- parallel region and thread IDs ====="
echo "============================================================"
$CC "$SRC/hello_omp.c" -o hello_omp
for t in 1 2 4 8; do
    echo "--- OMP_NUM_THREADS=$t, run 1"; OMP_NUM_THREADS=$t ./hello_omp
    echo "--- OMP_NUM_THREADS=$t, run 2"; OMP_NUM_THREADS=$t ./hello_omp
done
echo "--- Clause experiment: -DCLAUSE_THREADS=3 with OMP_NUM_THREADS=8"
$CC -DCLAUSE_THREADS=3 "$SRC/hello_omp.c" -o hello_clause
OMP_NUM_THREADS=8 ./hello_clause

echo; echo "============================================================"
echo "===== Task 2: vecadd.c -- parallel for and speedup ====="
echo "============================================================"
$CC "$SRC/vecadd.c" -o vecadd
for t in 1 2 4 8; do
    for r in 1 2 3; do OMP_NUM_THREADS=$t ./vecadd; done
done
echo "--- Scoping experiment: drop n from shared(...) with default(none) (must fail)"
$CC -DDROP_N_FROM_SHARED "$SRC/vecadd.c" -o vecadd_bad 2>&1 | head -12 || true

echo; echo "============================================================"
echo "===== Task 3: scoping_bug.c -- missing private and two fixes ====="
echo "============================================================"
$CC0 "$SRC/scoping_bug.c" -o bug
$CC0 -DFIX_PRIVATE "$SRC/scoping_bug.c" -o fix_private
$CC0 -DFIX_LOCAL   "$SRC/scoping_bug.c" -o fix_local
echo "--- Buggy version, 10 runs with 4 threads"
for i in $(seq 1 10); do OMP_NUM_THREADS=4 ./bug; done
echo "--- Buggy version, 1 thread"
OMP_NUM_THREADS=1 ./bug
echo "--- FIX_PRIVATE, 10 runs with 4 threads"
for i in $(seq 1 10); do OMP_NUM_THREADS=4 ./fix_private; done
echo "--- FIX_LOCAL, 10 runs with 4 threads"
for i in $(seq 1 10); do OMP_NUM_THREADS=4 ./fix_local; done
echo "--- DEFAULT_NONE build (must fail to compile)"
$CC0 -DDEFAULT_NONE "$SRC/scoping_bug.c" -o bug_none 2>&1 | head -12 || true

echo; echo "============================================================"
echo "===== Task 4: omp_sections.c -- independent sections ====="
echo "============================================================"
$CC "$SRC/omp_sections.c" -o omp_sections -lm
for t in 1 2 3 4 8; do
    echo "--- OMP_NUM_THREADS=$t"; OMP_NUM_THREADS=$t ./omp_sections
done
echo "--- 3 threads, 3 runs (thread-assignment check)"
for i in 1 2 3; do OMP_NUM_THREADS=3 ./omp_sections; done

echo; echo "===== Done ====="
