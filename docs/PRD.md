# PRD — nsh: A Unix Shell with Job Control and Built-in Observability

## 1. Pitch
nsh is a Unix shell written from scratch in C using real POSIX syscalls. It supports command execution, pipelines, I/O redirection, and full job control. Unlike a minimal teaching shell, it also **exposes its own internals**: a `trace` mode shows every fork/exec/process-group/signal event, and `stats` reports per-job resource usage.

## 2. Goals
- Correct, demoable shell that behaves like bash for the supported feature set.
- Every feature maps to a core OS concept (processes, IPC, signals, terminal control).
- Measured results: benchmark nsh against bash and dash.

## 3. In Scope (FIXED — do not add to this list)
| Area | Features |
|---|---|
| Parsing | Tokenizer, single/double quotes, backslash escapes, operators `\| < > >> &` |
| Execution | `fork` + `execvp`, exit status tracking, `$?` |
| Variables | `$VAR` expansion, `export VAR=value`, `$?` |
| Redirection | `<`, `>`, `>>` |
| Pipes | `cmd1 \| cmd2 \| ... \| cmdN` (arbitrary length) |
| Job control | `&`, `jobs`, `fg`, `bg`, Ctrl+Z (SIGTSTP), Ctrl+C (SIGINT) |
| Built-ins | `cd`, `exit`, `export`, `jobs`, `fg`, `bg`, `trace`, `stats` |
| Observability | `trace on/off` event log; `stats` per-job wall/user/sys time + max RSS |
| Benchmarks | nsh vs bash vs dash: spawn latency, pipeline throughput |

## 4. Out of Scope
- Scripting constructs (if/while/for/functions), `&&`, `||`, `;` chains
- Globbing / wildcards (stretch only)
- Here-documents, command substitution `$(...)`, subshells `( )`
- Full POSIX compliance
- Windows native support (use Linux / WSL2)

## 5. Stretch Goals (only if all core deliverables are done)
Command history with arrow keys, tab completion, basic globbing, `.nshrc`.

## 6. Success Criteria
- All items in TESTING.md pass; Valgrind clean.
- Ctrl+C / Ctrl+Z affect only the foreground job; the shell never dies or hangs.
- No zombie processes after any scenario (`ps` check).
- Benchmark report with at least 2 experiments and charts.

## 7. Deliverables
- [x] Design docs (this folder), committed in week 1
- [x] Tokenizer + parser with tests
- [x] Executor, built-ins, `$VAR` expansion
- [x] Redirection and multi-stage pipes
- [x] Job control + signal handling
- [x] Observability (`trace`, `stats`)
- [x] Test suite (bash comparison)
- [x] Benchmark scripts + results
- [x] README with build/run + demo recording (asciinema/GIF)
- [x] Project report with per-member contributions
