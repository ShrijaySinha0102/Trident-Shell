#include "executor.h"
#include "signals.h"
#include "builtins.h"
#include "jobs.h"
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "util.h"

extern int g_last_status;

/* Opens `path` with `flags` and makes it the process's descriptor `target`
 * (0 = stdin, 1 = stdout). Returns 0 on success, -1 on error (message printed). */
static int redirect_fd(const char *path, int flags, int target) {
    int fd = open(path, flags, 0666);
    if (fd < 0) {
        fprintf(stderr, "nsh: %s: %s\n", path, strerror(errno));
        return -1;
    }
    /* dup2 makes `target` a copy of fd; the original fd is then redundant and
     * must be closed, otherwise it leaks. Skip if open() already returned target. */
    if (fd != target) {
        if (dup2(fd, target) < 0) {
            perror("nsh: dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

/* Apply a command's redirections in order (later ones win, like bash). */
static int apply_redirections(Command *cmd) {
    for (size_t i = 0; i < cmd->nredirs; i++) {
        Redir *r = &cmd->redirs[i];
        int rc;
        if (r->type == REDIR_IN)
            rc = redirect_fd(r->path, O_RDONLY, 0);
        else if (r->type == REDIR_OUT)
            rc = redirect_fd(r->path, O_WRONLY | O_CREAT | O_TRUNC, 1);
        else
            rc = redirect_fd(r->path, O_WRONLY | O_CREAT | O_APPEND, 1);
        if (rc < 0) return -1;
    }
    return 0;
}

/* Runs inside a forked child; never returns. Pipe wiring is already done,
 * so explicit redirections applied here override the pipe (as in bash). */
static void run_stage(Command *cmd) {
    signals_reset_child();
    if (apply_redirections(cmd) < 0) _exit(1);
    if (!cmd->argv[0]) _exit(0);
    if (is_builtin(cmd->argv[0])) {
        int st = run_builtin(cmd);
        fflush(NULL);
        _exit(st);
    }
    execvp(cmd->argv[0], cmd->argv);
    fprintf(stderr, "nsh: %s: command not found\n", cmd->argv[0]);
    _exit(127);
}

/* Wait for EVERY stage so no zombies remain; the pipeline's status is the
 * status of the LAST stage. */
static int wait_foreground(Job *j) {
    int tty = isatty(STDIN_FILENO);
    size_t n = j->npids;
    int last = 0, stopped = 0;
    if (tty) tcsetpgrp(STDIN_FILENO, j->pgid);
    for (size_t k = 0; k < n; k++) {
        int st;
        pid_t r;
        do { r = waitpid(j->pids[k], &st, WUNTRACED); } while (r < 0 && errno == EINTR);
        if (r < 0) continue;
        if (WIFSTOPPED(st)) { stopped = 1; last = 128 + WSTOPSIG(st); }
        else if (k == n - 1)
            last = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
    }
    if (tty) tcsetpgrp(STDIN_FILENO, getpgrp());
    g_last_status = last;
    if (stopped) {
        j->state = JOB_STOPPED;
        j->background = 1;
        fprintf(stderr, "\n[%d] Stopped                 %s\n", j->id, j->cmdline);
    } else {
        job_remove(j);
    }
    return last;
}

/* N commands -> N-1 pipes -> N children in ONE process group. */
static int execute_pipeline(Pipeline *p) {
    size_t n = p->ncmds;
    pid_t *pids = xmalloc(n * sizeof(pid_t));
    int prev_read = -1;     /* read end of the previous pipe */
    pid_t pgid = 0;
    size_t started = 0;
    int failed = 0;

    fflush(NULL);           /* avoid children re-printing buffered output */
    for (size_t i = 0; i < n; i++) {
        int fd[2] = {-1, -1};
        if (i < n - 1 && pipe(fd) < 0) { perror("nsh: pipe"); failed = 1; break; }
        pid_t pid = fork();
        if (pid < 0) {
            perror("nsh: fork");
            if (fd[0] >= 0) { close(fd[0]); close(fd[1]); }
            failed = 1;
            break;
        }
        if (pid == 0) {
            setpgid(0, pgid);                       /* pgid 0 => new group */
            if (!p->background && isatty(STDIN_FILENO))   /* also in child: closes the */
                tcsetpgrp(STDIN_FILENO, pgid ? pgid : getpid()); /* tty race */
            if (prev_read >= 0 && dup2(prev_read, 0) < 0) _exit(1);
            if (fd[1] >= 0 && dup2(fd[1], 1) < 0) _exit(1);
            /* After dup2 the originals are duplicates: close them all, or
             * a reader never sees EOF and the pipeline hangs. */
            if (prev_read >= 0) close(prev_read);
            if (fd[0] >= 0) close(fd[0]);
            if (fd[1] >= 0) close(fd[1]);
            run_stage(&p->cmds[i]);
        }
        if (i == 0) pgid = pid;
        setpgid(pid, pgid);                         /* both sides: avoids race */
        pids[started++] = pid;
        if (prev_read >= 0) close(prev_read);
        if (fd[1] >= 0) close(fd[1]);
        prev_read = fd[0];
    }
    if (prev_read >= 0) close(prev_read);

    if (failed) {
        for (size_t k = 0; k < started; k++) waitpid(pids[k], NULL, 0);
        free(pids);
        return 1;
    }

    Job *j = job_add(pgid, pids, n, p->raw ? p->raw : p->cmds[0].argv[0],
                     p->background);
    free(pids);             /* job_add keeps its own copy */
    if (!j) return 1;
    if (p->background) {
        fprintf(stderr, "[%d] %d\n", j->id, (int)pgid);
        return 0;
    }
    return wait_foreground(j);
}

int execute(Pipeline *p) {
    if (!p || p->ncmds == 0) return 0;

    if (p->ncmds == 1) {
        Command *cmd = &p->cmds[0];
        if (!cmd->argv || !cmd->argv[0]) return 0;

        if (is_builtin(cmd->argv[0])) {
            int saved_in = dup(0);
            int saved_out = dup(1);
            if (apply_redirections(cmd) < 0) {
                dup2(saved_in, 0); close(saved_in);
                dup2(saved_out, 1); close(saved_out);
                g_last_status = 1;
                return 1;
            }
            int st = run_builtin(cmd);
            g_last_status = st;
            dup2(saved_in, 0); close(saved_in);
            dup2(saved_out, 1); close(saved_out);
            return st;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        } else if (pid == 0) {
            // Child
            signals_reset_child();
            if (p->background) {
                setpgid(0, 0);
            }
            if (apply_redirections(cmd) < 0) {
                _exit(1);
            }
            execvp(cmd->argv[0], cmd->argv);
            fprintf(stderr, "nsh: %s: command not found\n", cmd->argv[0]);
            _exit(127);
        } else {
            // Parent
            setpgid(pid, pid);
            if (p->background) {
                Job *j = job_add(pid, &pid, 1, p->raw ? p->raw : cmd->argv[0], 1);
                if (j) fprintf(stderr, "[%d] %d\n", j->id, pid);
                return 0;
            } else {
                Job *j = job_add(pid, &pid, 1, p->raw ? p->raw : cmd->argv[0], 0);
                return job_put_fg(j, 0);
            }
        }
    }

    return execute_pipeline(p);
}