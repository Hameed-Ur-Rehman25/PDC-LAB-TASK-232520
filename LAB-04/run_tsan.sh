#!/bin/bash
# Lab 04 - Task 2: Detecting the race with ThreadSanitizer
# Recompiles the UNCHANGED Task 1 program (race.c) with TSan and runs it.
# TSan writes its report to stderr; the first run is saved to tsan_report.txt.
# Must be run on Linux (WSL or a container) with GCC.
set -u
SRC="$(cd "$(dirname "$0")" && pwd)"
BUILD=/tmp/lab04
mkdir -p "$BUILD" && cd "$BUILD"

# -fsanitize=thread : instrument every memory access for race detection
# -g                : debug info so the report shows file names and line numbers
# -O1               : light optimization, recommended for TSan builds
# -DINCREMENTS_PER_THREAD=100000 : smaller workload, since TSan is 5-15x slower
gcc -fsanitize=thread -g -O1 -pthread -DINCREMENTS_PER_THREAD=100000 \
    "$SRC/race.c" -o race_tsan

./race_tsan 2> "$SRC/tsan_report.txt"
echo "(full report of this run saved to tsan_report.txt)"

# Run at least three times to see whether the report and final value change.
for i in 1 2 3; do
    echo "--- TSan run $i"
    ./race_tsan 2> tsan_run.txt
    echo "warnings: $(grep -c 'WARNING: ThreadSanitizer' tsan_run.txt)"
    grep -E 'SUMMARY' tsan_run.txt
done
