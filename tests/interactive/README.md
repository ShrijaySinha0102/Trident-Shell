# Interactive pty-based Tests

These tests use pseudo-terminals (`pty`) to verify terminal hijacking, process groups, and signals (`SIGINT`, `SIGTSTP`, `SIGCONT`) across foreground and background jobs:

1. `python3 tests/interactive/pipeline_signals.py`
   - Verifies pipeline process group creation (`setpgid`).
   - Verifies Ctrl+C delivers to all stages of a pipeline without killing the shell.
   - Verifies Ctrl+Z stops every stage (`SIGTSTP`).
   - Verifies `fg` resumes all stopped pipeline stages (`SIGCONT`).

2. `python3 tests/interactive/job_control_bg.py`
   - Verifies single-job Ctrl+Z stop.
   - Verifies `bg` resumes execution in the background.
   - Verifies `jobs` builtin reports running status.
   - Verifies `fg` brings the job back to foreground and terminal control.

