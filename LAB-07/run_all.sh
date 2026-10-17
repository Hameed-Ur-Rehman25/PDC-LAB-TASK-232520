#!/bin/bash
# Runs every Lab 07 task in order. Must be run on Linux (WSL or a container) with
# GCC and OpenMP (libgomp). Binaries are built in /tmp/lab07 so the source folder
# stays clean.
set -u
SRC="$(cd "$(dirname "$0")" && pwd)"
BUILD=/tmp/lab07
mkdir -p "$BUILD" && cd "$BUILD"

CC="gcc -O2 -fopenmp -std=gnu11"

echo "===== Toolchain ====="
gcc --version | head -1
echo "nproc: $(nproc)"
echo "cache line (LEVEL1_DCACHE_LINESIZE): $(getconf LEVEL1_DCACHE_LINESIZE)"
echo 'int main(void){return 0;}' > omp_test.c
gcc -fopenmp omp_test.c -o omp_test && ./omp_test && echo "OpenMP OK"

echo; echo "============================================================"
echo "===== Task 1: reduction.c -- reduction vs critical ====="
echo "============================================================"
$CC "$SRC/reduction.c" -o reduction
echo "--- Main run with 4 threads"
OMP_NUM_THREADS=4 ./reduction
echo "--- Scaling run: t = 1 2 4 8"
for t in 1 2 4 8; do echo "(OMP_NUM_THREADS=$t)"; OMP_NUM_THREADS=$t ./reduction; done
echo "--- Two more runs with 4 threads (racy repeatability, Q1)"
OMP_NUM_THREADS=4 ./reduction
OMP_NUM_THREADS=4 ./reduction

echo; echo "============================================================"
echo "===== Task 2: schedule_bench.c -- scheduling on an uneven workload ====="
echo "============================================================"
$CC "$SRC/schedule_bench.c" -o schedule_bench
for t in 2 4 8; do echo "--- OMP_NUM_THREADS=$t"; OMP_NUM_THREADS=$t ./schedule_bench; done
echo "--- Optional: single policy from OMP_SCHEDULE=dynamic,16 (4 threads)"
OMP_NUM_THREADS=4 OMP_SCHEDULE="dynamic,16" ./schedule_bench | head -2

echo; echo "============================================================"
echo "===== Task 3: false_sharing.c -- false sharing and padding ====="
echo "============================================================"
$CC "$SRC/false_sharing.c" -o false_sharing
echo "--- Three runs with 4 threads"
for i in 1 2 3; do OMP_NUM_THREADS=4 ./false_sharing; done
echo "--- Effect of thread count: t = 1 2 4 8"
for t in 1 2 4 8; do echo "(OMP_NUM_THREADS=$t)"; OMP_NUM_THREADS=$t ./false_sharing; done

echo; echo "============================================================"
echo "===== Task 4: schedule_bench.c -DUNIFORM -- uniform workload ====="
echo "============================================================"
$CC -DUNIFORM "$SRC/schedule_bench.c" -o schedule_bench_uniform
echo "--- Uniform workload with 4 threads"
OMP_NUM_THREADS=4 ./schedule_bench_uniform

echo; echo "===== Done ====="
