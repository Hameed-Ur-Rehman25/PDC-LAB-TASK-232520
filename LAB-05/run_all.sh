#!/bin/bash
# Runs every Lab 05 task in order. Must be run on Linux (WSL or a container) with GCC.
# Binaries are built in /tmp/lab05 so the source folder stays clean.
set -u
SRC="$(cd "$(dirname "$0")" && pwd)"
BUILD=/tmp/lab05
mkdir -p "$BUILD" && cd "$BUILD"

OPT="gcc -O2 -pthread -std=gnu11"
TSAN="gcc -fsanitize=thread -g -O1 -pthread -std=gnu11"

# Runs a TSan binary and prints its normal output plus the number of warnings.
tsan_run() {
    "./$1" 2> "$1.txt"
    echo "TSan warnings: $(grep -c 'WARNING: ThreadSanitizer' "$1.txt")"
}

# Runs a program that is expected to hang and reports how it ended.
hang_run() {
    timeout 5 "./$1"
    code=$?
    if [ $code -eq 124 ]; then echo "exit code 124: killed by timeout after 5 s (no progress)"
    else echo "exit code $code"; fi
}

echo "===== Toolchain ====="
gcc --version | head -1
echo "nproc: $(nproc)"
echo 'int main(void){return 0;}' > tsan_test.c
gcc -fsanitize=thread tsan_test.c -o tsan_test && ./tsan_test && echo "TSan OK"

echo; echo "===== Task 1: buffer_mutex.c (buffer 8), 5 runs ====="
$OPT "$SRC/buffer_mutex.c" -o buffer_mutex
for i in $(seq 1 5); do ./buffer_mutex; done
echo "--- TSan build (ITEMS=20000)"
$TSAN -DITEMS=20000 "$SRC/buffer_mutex.c" -o buffer_mutex_tsan
tsan_run buffer_mutex_tsan
echo "--- Buffer size 1 and 64"
$OPT -DBUFFER_SIZE=1 "$SRC/buffer_mutex.c" -o buffer_mutex_1 && ./buffer_mutex_1
$OPT -DBUFFER_SIZE=64 "$SRC/buffer_mutex.c" -o buffer_mutex_64 && ./buffer_mutex_64

echo; echo "===== Task 2: buffer_sem.c, 3 runs per configuration ====="
for cfg in "1 1 8" "2 2 8" "4 4 8" "4 4 1" "4 4 64" "4 2 8"; do
    set -- $cfg
    exe=buffer_sem_$1_$2_$3
    $OPT -DNUM_PRODUCERS=$1 -DNUM_CONSUMERS=$2 -DBUFFER_SIZE=$3 "$SRC/buffer_sem.c" -o $exe
    for i in 1 2 3; do ./$exe; done
done
echo "--- TSan build (4/4/8, ITEMS_PER_PRODUCER=5000)"
$TSAN -DNUM_PRODUCERS=4 -DNUM_CONSUMERS=4 -DBUFFER_SIZE=8 -DITEMS_PER_PRODUCER=5000 \
    "$SRC/buffer_sem.c" -o buffer_sem_tsan
tsan_run buffer_sem_tsan
echo "--- Deadlock experiment: consumer locks buf_lock before sem_wait"
$OPT -DSWAP_LOCK_ORDER "$SRC/buffer_sem.c" -o buffer_sem_swapped
hang_run buffer_sem_swapped
echo "--- Comparison with Task 1 (Q5): 1/1/8 with 1,000,000 items, 3 runs"
$OPT -DNUM_PRODUCERS=1 -DNUM_CONSUMERS=1 -DBUFFER_SIZE=8 -DITEMS_PER_PRODUCER=1000000 \
    "$SRC/buffer_sem.c" -o buffer_sem_cmp
for i in 1 2 3; do ./buffer_sem_cmp; done

echo; echo "===== Task 3: cond_signal.c, 4 scenarios ====="
$OPT "$SRC/cond_signal.c" -o cond_a
$OPT -DSIGNAL_FIRST "$SRC/cond_signal.c" -o cond_b
$OPT -DNUM_WORKERS=3 "$SRC/cond_signal.c" -o cond_c
$OPT -DUSE_SPIN "$SRC/cond_signal.c" -o cond_d
for v in a b c d; do echo "--- cond_$v"; ./cond_$v; done
echo "--- TSan build (variant A, DELAY_MS=200)"
$TSAN -DDELAY_MS=200 "$SRC/cond_signal.c" -o cond_tsan
tsan_run cond_tsan
echo "--- Lost-signal experiment: variant A without the signal call"
$OPT -DNO_SIGNAL "$SRC/cond_signal.c" -o cond_nosignal
hang_run cond_nosignal
echo "--- Extra (Q5): 3 workers with pthread_cond_signal instead of broadcast"
$OPT -DNUM_WORKERS=3 -DSIGNAL_ONE "$SRC/cond_signal.c" -o cond_signal_one
hang_run cond_signal_one

echo; echo "===== Task 4: phase_barrier.c (4 workers), 5 runs per version ====="
$OPT "$SRC/phase_barrier.c" -o phase_barrier
$OPT -DNO_BARRIER "$SRC/phase_barrier.c" -o phase_nobar
for i in $(seq 1 5); do ./phase_barrier; done
for i in $(seq 1 5); do ./phase_nobar; done
echo "--- TSan build (ROUNDS=2000)"
$TSAN -DROUNDS=2000 "$SRC/phase_barrier.c" -o phase_tsan
tsan_run phase_tsan
echo "--- 8 workers"
$OPT -DNUM_WORKERS=8 "$SRC/phase_barrier.c" -o phase_barrier_8 && ./phase_barrier_8

echo; echo "===== Task 5: lockfree_stack.c (4 threads), 5 runs ====="
$OPT "$SRC/lockfree_stack.c" -o lockfree_stack
for i in $(seq 1 5); do ./lockfree_stack; done
echo "--- TSan build (NODES_PER_THREAD=5000)"
$TSAN -DNODES_PER_THREAD=5000 "$SRC/lockfree_stack.c" -o lockfree_tsan
tsan_run lockfree_tsan
echo "--- 8 threads"
$OPT -DNUM_THREADS=8 "$SRC/lockfree_stack.c" -o lockfree_stack_8 && ./lockfree_stack_8
echo "--- Mutex stack for comparison (Q5): 4 threads x5, 8 threads x1"
$OPT -DUSE_MUTEX "$SRC/lockfree_stack.c" -o mutex_stack
for i in $(seq 1 5); do ./mutex_stack; done
$OPT -DUSE_MUTEX -DNUM_THREADS=8 "$SRC/lockfree_stack.c" -o mutex_stack_8 && ./mutex_stack_8
