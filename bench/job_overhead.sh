#!/usr/bin/env bash
# Job-control overhead: launch and reap N background sleep 0.05 jobs
N=${1:-50}
cd "$(dirname "$0")/.." || exit 1

echo "Job control overhead benchmark (N=$N jobs):"
printf "%-12s %s\n" "Shell" "Time (s)"

for sh in "bash --norc" "./nsh"; do
    script=$(mktemp)
    for _ in $(seq "$N"); do
        echo 'sleep 0.02 &'
    done > "$script"
    echo 'sleep 0.5' >> "$script"

    s=$(date +%s.%N)
    $sh < "$script" > /dev/null 2>&1
    e=$(date +%s.%N)
    rm -f "$script"
    dur=$(echo "$e - $s" | bc)
    printf "%-12s %s s\n" "$sh" "$dur"
done
