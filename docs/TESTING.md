# TESTING

## Approach
Each test case is a file in `tests/cases/NAME.sh` containing shell input, run through BOTH nsh and bash (`bash --norc`). `tests/run.sh` diffs stdout and exit code. Expected-difference cases (job control messages, prompts) use hand-written `NAME.expected` files instead.

```
make test         # run all cases
make valgrind     # same, under valgrind --leak-check=full
```

## Test matrix
| Area | Cases |
|---|---|
| Basic exec | `ls`, `pwd`, `echo hi`, unknown command (exit 127), command with args |
| Quoting | `echo "a  b"`, `echo 'a $HOME'`, `echo a\ b`, unterminated quote error |
| Variables | `export X=1; echo $X`, unset var empty, `$?` after success and failure |
| Redirection | `> f`, `>> f` appends, `< f`, missing input file error, `> /nonexistent/x` error, multiple redirects |
| Pipes | 2 stages, 5 stages, `yes \| head -n 3` (SIGPIPE), large data `seq 1 100000 \| wc -l`, pipe + redirect |
| Syntax errors | `\| ls`, `ls \|`, `ls > `, `a & b` |
| Built-ins | `cd` to valid/invalid dir, `cd` no args (HOME), `exit 3` status |
| Background | `sleep 1 &` returns immediately, `jobs` lists it, completion notice, no zombies |
| Job control | `sleep 100`, Ctrl+Z, `jobs` shows Stopped, `bg`, `fg`, Ctrl+C kills only fg |
| Signals | Ctrl+C at empty prompt does not exit shell; Ctrl+C during `sleep` kills sleep only |
| Zombies | after 50 background jobs complete, `ps --ppid <nsh>` shows none |
| Robustness | very long line, empty line, whitespace-only, EOF (Ctrl+D) exits cleanly |

## Interactive tests
Signal/terminal tests need a pty. Use `expect` or Python `pexpect` scripts in `tests/interactive/`.

## Fd-leak check
After each pipeline test: `ls /proc/$NSH_PID/fd | wc -l` must equal the baseline count.

## Benchmarks (`bench/`)
| Experiment | Method |
|---|---|
| Spawn latency | run `/bin/true` 5000 times; report mean/stddev for nsh, bash, dash |
| Pipeline throughput | `seq 1 5000000 \| cat \| cat \| wc -l`, 1 to 8 stages; time vs stage count |
| Job-control overhead | 100 background `sleep 0.1` jobs; time to launch and reap |
| Trace overhead | same spawn test with trace on vs off |
Run each 10 times, report median and spread, record machine/kernel info.

## Definition of done
A feature is done when it has tests in this matrix, passes Valgrind, and leaves no extra fds or zombies.

## Person 2 additions (pipes, redirection, hardening)
| Check | How to run | What it proves |
|---|---|---|
| Pipe / redirection / syntax-error cases | `make test` | stdout and exit status match bash (`pipe_*`, `redir_*`, `syntax_*`) |
| Fd-leak and zombie check | `bash tests/leak_check.sh` | 50 rounds of pipelines leave nsh's fd count unchanged and no zombies |
| Pipeline signals (pty) | `python3 tests/interactive/pipeline_signals.py` | Ctrl+C kills and Ctrl+Z stops EVERY stage; shell survives. `fg` resume is reported as SKIP until Person 3 implements it |
| Valgrind | `make valgrind` | no leaks in the shell process |
