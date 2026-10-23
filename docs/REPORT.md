# Final Project Report — nsh (Trident-Shell)
**A Unix Shell with Job Control and Built-in Process Observability in C**

---

## 1. Executive Summary

`nsh` is an industrial-quality, POSIX-compliant Unix shell engineered from the ground up in standard C (C11) using low-level Unix system calls (`fork`, `execvp`, `pipe`, `dup2`, `setpgid`, `tcsetpgrp`, `wait4`, `sigaction`). In addition to standard shell features—such as command parsing, variable expansion, multi-stage pipelines, arbitrary redirections, and full terminal job control (`&`, `jobs`, `fg`, `bg`, `Ctrl+C`, `Ctrl+Z`)—`nsh` incorporates built-in **process observability**:
1. **Interactive Trace Engine (`trace on / off`)**: Exposes shell runtime events (`fork`, `exec`, `setpgid`, `pipe`, `tcsetpgrp`, `sigcont`, `wait4`) in real time to stderr.
2. **Process Resource Accounting (`stats`)**: Harvests kernel resource metrics (`wall time`, `user CPU`, `system CPU`, and peak `max RSS`) via `wait4()` and displays clean telemetry tables on demand.

All eight development phases have been successfully completed, verified through an automated 41-case differential test suite comparing against `bash`, full Valgrind memory leak verification, background zombie stress checks, and interactive pseudo-terminal (`pty`) signal tests.

---

## 2. System Architecture & Component Design

### 2.1 Architectural Flow Diagram

```
                 +---------------------------+
                 |  Terminal Input / Stdin   |
                 +-------------+-------------+
                               |
                               v
                 +---------------------------+
                 |    Lexer (lexer.c)        |  --> TokenList (words, pipes, redirs, &)
                 +-------------+-------------+
                               |
                               v
                 +---------------------------+
                 |   Expander (expand.c)     |  --> $VAR, ${VAR}, and $? expansion
                 +-------------+-------------+
                               |
                               v
                 +---------------------------+
                 |    Parser (parser.c)      |  --> Pipeline (Commands, Redirs, Background)
                 +-------------+-------------+
                               |
                               v
                 +---------------------------+
                 |   Executor (executor.c)   |
                 +------+--------------+-----+
                        |              |
      [Single Builtin]  |              |  [External / Multi-Stage Pipeline]
                        v              v
            +----------------+   +------------------------------------+
            | builtins.c     |   | For each stage:                    |
            | cd, exit,      |   |  - pipe()                          |
            | export, jobs,  |   |  - fork()                          |
            | fg, bg, trace, |   |  - setpgid(pid, pgid) [both sides] |
            | stats          |   |  - dup2() & close unused ends      |
            +----------------+   |  - signals_reset_child()           |
                                 |  - execvp()                        |
                                 +-----------------+------------------+
                                                   |
                                                   v
                                 +------------------------------------+
                                 | Jobs & Signals (jobs.c, signals.c) |
                                 |  - job_add()                       |
                                 |  - tcsetpgrp() [foreground]        |
                                 |  - wait4() rusage collection       |
                                 |  - tcsetpgrp(shell) reclamation    |
                                 +-----------------+------------------+
                                                   |
                                                   v
                                 +------------------------------------+
                                 | Observability (trace.c, stats.c)   |
                                 |  - trace_event() logging           |
                                 |  - stats_record() & stats_print()  |
                                 +------------------------------------+
```

### 2.2 Core Modules & Interfaces

| Module | Files | Responsibility |
|---|---|---|
| **Tokenizer & Lexer** | `lexer.c`, `lexer.h` | Finite state machine tokenizing words, handling `'...'` literal strings, `"..."` double quotes with escapes (`\"`, `\\`), backslash escapes, and operators (`\|`, `<`, `>`, `>>`, `&`). |
| **Parameter Expander** | `expand.c`, `expand.h` | Expands environment parameters (`$VAR`, `${VAR}`) and previous exit codes (`$?`) while respecting single-quote masking. |
| **Recursive Parser** | `parser.c`, `parser.h` | Converts tokens into structured `Pipeline` and `Command` representations; associates I/O redirections per stage; validates syntax errors (`nsh: syntax error near '|'`). |
| **Pipeline Executor** | `executor.c`, `executor.h` | Orchestrates process creation, pipe allocation, descriptor replication (`dup2`), explicit redirection overrides, and child exec. |
| **Job & Process Table** | `jobs.c`, `jobs.h` | Maintains process groups, background state, pipeline process tracking, and terminal handoff via `job_put_fg` / `job_put_bg`. |
| **Signal Dispatcher** | `signals.c`, `signals.h` | Configures signal masks via POSIX `sigaction`; ensures shell ignores job control signals while children reset to default behaviors. |
| **Observability Subsystem**| `trace.c`, `stats.c` | Non-intrusive runtime event logger and kernel resource usage accumulator using `wait4()`. |
| **Utility Infrastructure** | `util.c`, `util.h` | Safe dynamic memory allocation (`xmalloc`, `xrealloc`, `xstrdup`) with abort-on-OOM semantics. |

---

## 3. Deep Dive: Key Engineering Challenges & Solutions

### 3.1 Process Groups & Terminal Ownership Race Conditions
- **Challenge:** When a child process is spawned, a race condition exists between the parent setting the process group with `setpgid(pid, pgid)` and the child calling `tcsetpgrp(STDIN_FILENO, pgid)` or `execvp()`. If the child attempts `tcsetpgrp` before `setpgid` runs in the parent, or parent calls `tcsetpgrp` before child sets its group, the call fails with `EPERM`.
- **Solution:** `setpgid` is called synchronously in **both** the parent and child immediately after `fork()`. The first stage sets its PGID to its own PID (`setpgid(0, 0)`), establishing the leader, and subsequent stages join this group. Foreground pipelines immediately claim the controlling terminal in both parent and child, eliminating timing windows.

### 3.2 Terminal Reclamation & The SIGTTOU Trap
- **Challenge:** When a background process group or a returning shell invokes `tcsetpgrp()` to reclaim foreground terminal control, the Linux kernel generates a `SIGTTOU` signal. By default, `SIGTTOU` stops the calling process, causing the shell to hang permanently.
- **Solution:** During initialization in `signals_init()`, the shell configures `SIGTTOU` and `SIGTTIN` to `SIG_IGN` via `sigaction`. When the foreground job exits or stops, the shell safely re-acquires terminal control (`tcsetpgrp(STDIN_FILENO, getpgrp())`) without kernel intervention.

### 3.3 Pipe Descriptor Leaks and Deadlocks
- **Challenge:** If any write end of an intermediate pipe remains unclosed in the shell or any stage, the consumer process will never receive `EOF` (`read() == 0`), leading to permanent hangs on commands like `cat`, `sort`, or `wc`.
- **Solution:** Strict descriptor lifecycle management:
  1. Each stage duplicates `prev_read` to `STDIN_FILENO` and `fd[1]` to `STDOUT_FILENO`.
  2. The child immediately closes the original file descriptors before calling `execvp()`.
  3. The parent closes the write end `fd[1]` immediately after `fork()` and retains `fd[0]` only until the subsequent child has duplicated it.
  4. Verified by `tests/leak_check.sh` maintaining zero descriptor leaks across 50 consecutive pipeline stress rounds.

### 3.4 Multi-Stage Background Pipeline Reaping
- **Challenge:** A background pipeline consists of $N$ processes. Reaping only one process (or searching only by PGID) causes zombie accumulation for remaining stages. Furthermore, rapid completion of multiple background jobs can coalesce `SIGCHLD` signals.
- **Solution:** The asynchronous signal handler only sets a volatile flag (`g_sigchld_pending = 1`). In the main REPL loop, `wait4(-1, &status, WNOHANG | WUNTRACED, &ru)` loops until no reaped children remain. Reaped PIDs update the process array within `Job`. Only when all stages (`npids`) have exited is the job transition marked `JOB_DONE`, notified to the user, and deallocated.

---

## 4. Verification & Hardening Results

### 4.1 Test Suite Matrix Coverage

`nsh` passes all 41 test cases against `bash --norc`:

```
./tests/run.sh
passed=41 failed=0
```

- **Basic Exec:** `basic_echo.sh`, `basic_pwd.sh`, `cmd_with_args.sh`, `unknown_cmd.sh`
- **Quoting & Escapes:** `quotes.sh`, `quotes_corner.sh`, `quotes_unterminated.sh`
- **Parameter Expansion:** `builtin_export.sh`, `expand_status.sh`, `var_unset.sh`
- **Redirection:** `redir_in.sh`, `redir_out.sh`, `redir_app.sh`, `redir_in_and_out.sh`, `redir_multi.sh`, `redir_in_missing.sh`, `redir_out_bad.sh`, `redir_out_bad_path.sh`
- **Pipelines:** `pipe_2stage.sh`, `pipe_4stage.sh`, `pipe_5stage.sh`, `pipe_large_data.sh`, `pipe_status_last.sh`, `pipe_unknown_middle.sh`, `pipe_bad_redirect_stage.sh`, `pipe_with_redirs.sh`, `pipe_sigpipe.sh`
- **Syntax Error Trapping:** `syntax_leading_pipe.sh`, `syntax_trailing_pipe.sh`, `syntax_missing_target.sh`
- **Built-ins:** `builtin_cd.sh`, `builtin_cd_home.sh`, `builtin_cd_invalid.sh`, `builtin_exit_status.sh`, `builtin_observability.sh`
- **Robustness:** `empty_line.sh`, `robustness_whitespace.sh`, `robustness_long_line.sh`, `comments.sh`

### 4.2 Interactive PTY Signal Tests

Two automated pseudo-terminal test scripts (`tests/interactive/`):
1. `pipeline_signals.py`:
   - `PASS pipeline started with 2 live stages`
   - `PASS shell survives Ctrl+C`
   - `PASS Ctrl+C killed the pipeline`
   - `PASS Ctrl+Z stopped all stages`
   - `PASS shell usable after Ctrl+Z`
   - `PASS fg resumed all stages`
   - `PASS shell alive at end`
   - `PASS no leftover processes`
2. `job_control_bg.py`:
   - `PASS Ctrl+Z stopped job`
   - `PASS bg resumed job`
   - `PASS jobs listed the job`
   - `PASS fg + Ctrl+C killed the job`
   - `PASS shell alive after job control`

### 4.3 Valgrind Memory Leak Audit

Running `make valgrind`:
```
NSH_WRAPPER="valgrind --leak-check=full --error-exitcode=99 -q" ./tests/run.sh
passed=41 failed=0
```
- Zero memory leaks detected (`0 bytes in 0 blocks`).
- Zero invalid reads/writes or uninitialized memory accesses.

### 4.4 File Descriptor & Zombie Stress Test

Running `bash tests/leak_check.sh`:
- 50 rounds of multi-stage pipelines and background tasks.
- Baseline file descriptors: **3** (`stdin`, `stdout`, `stderr`).
- Final file descriptors: **3** (zero descriptors leaked).
- Active zombies: **0** (all children cleanly harvested).

---

## 5. Performance Benchmarks & Empirical Evaluation

Benchmarking was performed against system `bash 5.2.21` and `dash 0.5.12` on Linux WSL2 (Linux 6.6.87.2, Intel Core i5-1235U). Full empirical data and SVG charts are published in `docs/BENCHMARKS.md`.

### Summary of Results:
1. **Spawn Latency (2,000 `/bin/true` spawns):**
   - **dash:** 0.871s
   - **nsh:** 1.107s
   - **bash:** 1.265s
   - *Result:* `nsh` is **14% faster than bash** and approaches the minimal footprint of `dash`.
2. **Pipeline Throughput (1,000,000 integers across 1 to 8 stages):**
   - 1 stage: `nsh` 0.0204s | `bash` 0.0199s | `dash` 0.0158s
   - 8 stages: `nsh` 0.0212s | `bash` 0.0216s | `dash` 0.0182s
   - *Result:* High throughput with consistent multi-process pipelining.
3. **Trace Overhead:**
   - With `trace off`, overhead is undetectable (<1%).
   - With `trace on`, overhead is only +4.6% despite comprehensive syscall event logging to stderr.

---

## 6. Team Contributions

| Team Member | Functional Areas & Phases | Key Deliverables |
|---|---|---|
| **Person 1** | Lexer, Parser, Built-ins, Expansion, Observability, Benchmarks (Phases 1, 2, 3, 5, 7) | - Lexer state machine (`lexer.c/h`)<br>- Grammar parser (`parser.c/h`)<br>- Parameter expander for `$VAR`, `${VAR}`, `$?` (`expand.c/h`)<br>- Core built-ins (`cd`, `exit`, `export`, `trace`, `stats`)<br>- `wait4` rusage accounting (`stats.c/h`)<br>- Trace event logging (`trace.c/h`)<br>- Comprehensive benchmark suite & charts (`bench/`) |
| **Person 2** | Executor, Pipelines, Redirection, Hardening, Test Harness (Phases 1, 2, 3, 4, 6) | - I/O redirection handlers (`<`, `>`, `>>`)<br>- N-stage pipeline executor with descriptor isolation (`executor.c/h`)<br>- Automated differential test harness (`tests/run.sh`)<br>- File descriptor leak and zombie stress testing (`tests/leak_check.sh`)<br>- Valgrind memory leak verification |
| **Person 3** | Job Control, Signal Handling, Process Groups (Phases 1, 2, 3, 4, 5, 6) | - Signal handlers with `sigaction` (`signals.c/h`)<br>- Process group leadership and terminal handoff (`jobs.c/h`)<br>- Built-ins `jobs`, `fg`, `bg`<br>- Interactive pseudo-terminal testing (`tests/interactive/`) |
