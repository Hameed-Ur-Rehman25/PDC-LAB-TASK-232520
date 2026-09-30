#!/bin/bash
# Runs every Lab 04 task in order. Must be run on Linux (WSL or a container) with GCC.
# Binaries are built in /tmp/lab04 so the source folder stays clean.
# Output: output.txt (all console output) and tsan_report.txt (Task 2 TSan report).
set -u
SRC="$(cd "$(dirname "$0")" && pwd)"
BUILD=/tmp/lab04
mkdir -p "$BUILD" && cd "$BUILD"

echo "===== Toolchain ====="
gcc --version | head -1
echo "nproc: $(nproc)"
echo 'int main(void){return 0;}' > tsan_test.c
gcc -fsanitize=thread tsan_test.c -o tsan_test && ./tsan_test && echo "TSan OK"

echo; echo "===== Task 1: race.c (-O0), 10 runs ====="
gcc -O0 -pthread "$SRC/race.c" -o race
for i in $(seq 1 10); do echo "--- Run $i"; ./race; done

echo; echo "===== Task 2: race.c with ThreadSanitizer ====="
bash "$SRC/run_tsan.sh"
cd "$BUILD"

echo; echo "===== Task 3: race_mutex.c (-O0), 10 runs ====="
gcc -O0 -pthread "$SRC/race_mutex.c" -o race_mutex
for i in $(seq 1 10); do echo "--- Run $i"; ./race_mutex; done
echo "--- TSan build"
gcc -fsanitize=thread -g -O1 -pthread -DINCREMENTS_PER_THREAD=100000 "$SRC/race_mutex.c" -o race_mutex_tsan
./race_mutex_tsan 2> tsan_mutex.txt
echo "TSan warnings: $(grep -c 'WARNING: ThreadSanitizer' tsan_mutex.txt)"

echo; echo "===== Task 4: counter_bench.c (-O2), 5 runs per version ====="
for v in MUTEX SYNC C11; do
    gcc -O2 -pthread -std=gnu11 -DUSE_$v "$SRC/counter_bench.c" -o bench_${v,,}
done
for v in mutex sync c11; do
    echo "--- bench_$v"
    for i in $(seq 1 5); do ./bench_$v; done
done
echo "--- TSan check of the atomic versions"
for v in SYNC C11; do
    gcc -fsanitize=thread -g -O1 -pthread -std=gnu11 -DUSE_$v -DINCREMENTS_PER_THREAD=100000 \
        "$SRC/counter_bench.c" -o bench_${v,,}_tsan
    ./bench_${v,,}_tsan 2> tsan_$v.txt
    echo "USE_$v TSan warnings: $(grep -c 'WARNING: ThreadSanitizer' tsan_$v.txt)"
done

echo; echo "===== Task 4 extra: thread-count sweep (Q5), 3 runs each ====="
for n in 1 8 16; do
    for v in MUTEX SYNC; do
        gcc -O2 -pthread -std=gnu11 -DUSE_$v -DNUM_THREADS=$n "$SRC/counter_bench.c" -o sweep
        echo "--- NUM_THREADS=$n USE_$v"
        for i in 1 2 3; do ./sweep; done
    done
done
