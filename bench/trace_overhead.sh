#!/usr/bin/env bash
# Trace overhead benchmark: run /bin/true N times with trace off vs trace on
N=${1:-2000}
cd "$(dirname "$0")/.." || exit 1

echo "Trace overhead benchmark (N=$N spawns):"
printf "%-16s %s\n" "Mode" "Time (s)"

# 1. Trace off
script_off=$(mktemp)
for _ in $(seq "$N"); do
    echo '/bin/true'
done > "$script_off"

s=$(date +%s.%N)
./nsh < "$script_off" > /dev/null 2>&1
e=$(date +%s.%N)
rm -f "$script_off"
dur_off=$(echo "$e - $s" | bc)
printf "%-16s %s s\n" "trace off" "$dur_off"

# 2. Trace on
script_on=$(mktemp)
echo 'trace on' > "$script_on"
for _ in $(seq "$N"); do
    echo '/bin/true'
done >> "$script_on"
echo 'trace off' >> "$script_on"

s=$(date +%s.%N)
./nsh < "$script_on" > /dev/null 2>&1
e=$(date +%s.%N)
rm -f "$script_on"
dur_on=$(echo "$e - $s" | bc)
printf "%-16s %s s\n" "trace on" "$dur_on"
