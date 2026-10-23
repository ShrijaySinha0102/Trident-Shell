# MEMORY — running log (update continuously; newest entries on top)

## Current status
- Week: Final Delivery
- Done: Phases 1 through 8 completed. 41/41 unit/differential tests pass, Valgrind clean, pty interactive signals pass, benchmarks executed with SVG charts, README & technical report written.
- In progress: Release packaging & tagging
- Blocked: None

## Decisions (append only)
| Date | Decision | Why | Who |
|---|---|---|---|
| wk3 | Unify execution path via `job_put_fg` | Eliminates duplicated wait logic across single commands and pipelines | all |
| wk3 | `#define _DEFAULT_SOURCE` in `jobs.c` & `main.c` | Exposes `wait4()` prototype under `-std=c11 -D_POSIX_C_SOURCE=200809L` | P1 |
| wk3 | Multi-stage background PID tracking in `Job` | Avoids orphan processes in background pipelines | P2 |
| wk3 | Support `${VAR}` syntax in `expand.c` | Standard bash parameter brace expansion compatibility | P1 |
| wk1 | Use `sigaction`, not `signal` | portable, defined semantics | all |
| wk1 | One process group per pipeline | Ctrl+C/Z must hit whole pipeline | P3 |
| wk1 | `wait4` for reaping | gives rusage for `stats` for free | P1 |
| wk1 | Out-of-scope list frozen (see PRD.md) | prevent scope creep | all |

## Gotchas / bugs found
| Date | Symptom | Cause | Fix |
|---|---|---|---|
| wk3 | Background pipeline stages became zombies | Reaping searched by PGID which only matched the leader PID | Replaced with `job_find_by_pid` and tracked remaining active PIDs |
| wk3 | Compilation warning with `wait4` under `-std=c11` | Glibc hides BSD extensions under strict POSIX standard flag | Added `#define _DEFAULT_SOURCE` before includes |
| wk3 | Pipeline signals pty test skipped fg resume | `fg`/`bg` builtins were stubbed | Fully implemented `fg`/`bg` with `kill(-pgid, SIGCONT)` and `tcsetpgrp` |
| wk2 | pipeline hangs on `cat` | write end of pipe left open in parent | close in parent after fork |

## Interface changes
| Date | Change | Affected people |
|---|---|---|
| wk3 | Added `Job *job_find_by_pid(pid_t pid);` to `jobs.h` | P2, P3 |
| wk3 | Added `signals_block_chld` / `signals_unblock_chld` to `signals.h` | P3 |

## Open questions
- Should `fg` with no args use the most recent job or the most recently stopped? (default: most recent, fallback to stopped)

