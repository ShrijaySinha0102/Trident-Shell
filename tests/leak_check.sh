#!/usr/bin/env bash
# Fd-leak and zombie check for pipelines. Run from the repo root: bash tests/leak_check.sh
# Idea: keep ONE nsh alive through a FIFO, run many pipelines, then inspect it from outside.
cd "$(dirname "$0")/.." || exit 1
fifo=$(mktemp -u /tmp/nsh_fifo.XXXXXX); mkfifo "$fifo"
./nsh < "$fifo" > /dev/null 2>&1 &
pid=$!
exec 3> "$fifo"                       # holding fd 3 open keeps nsh reading
sleep 0.3
before=$(ls /proc/$pid/fd | wc -l)

for i in $(seq 1 50); do
    echo 'seq 1 100 | cat | cat | wc -l > /dev/null' >&3
    echo 'echo hi | nosuchcmd_x 2> /dev/null' >&3
    echo 'cat < /no/such | cat 2> /dev/null' >&3
    echo 'sleep 0.05 | sleep 0.05 &' >&3
done
echo 'echo done > /dev/null' >&3
sleep 2                               # let background pipelines finish

after=$(ls /proc/$pid/fd | wc -l)
idle_zombies=$(ps -o stat= --ppid "$pid" | grep -c '^Z')   # while nsh sits idle
echo 'echo nudge > /dev/null' >&3                            # simulate "next prompt"
sleep 0.5
zombies=$(ps -o stat= --ppid "$pid" | grep -c '^Z')
[ "$idle_zombies" -gt 0 ] && echo "NOTE $idle_zombies zombie(s) while idle: SIGCHLD flag is only handled when nsh reads the next line"

exec 3>&-; wait "$pid" 2>/dev/null; rm -f "$fifo"
rc=0
[ "$before" -eq "$after" ] && echo "PASS fd count unchanged ($before -> $after)" || { echo "FAIL fd leak ($before -> $after)"; rc=1; }
[ "$zombies" -eq 0 ] && echo "PASS no zombie children" || { echo "FAIL $zombies zombie(s)"; rc=1; }
exit $rc
