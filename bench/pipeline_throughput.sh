#!/usr/bin/env bash
# Pipeline throughput: seq 1 N | cat | ... | wc -l across 1, 2, 4, 8 stages.
N=${1:-2000000}
cd "$(dirname "$0")/.." || exit 1

echo "Pipeline throughput benchmark (N=$N integers):"
printf "%-12s %-8s %s\n" "Shell" "Stages" "Time (s)"

for stages in 1 2 4 8; do
    pipeline="seq 1 $N"
    for ((s=1; s<stages; s++)); do
        pipeline="$pipeline | cat"
    done
    pipeline="$pipeline | wc -l"

    for sh in "bash --norc" "dash" "./nsh"; do
        s=$(date +%s.%N)
        echo "$pipeline" | $sh > /dev/null 2>&1
        e=$(date +%s.%N)
        dur=$(echo "$e - $s" | bc)
        printf "%-12s %-8d %s s\n" "$sh" "$stages" "$dur"
    done
done
