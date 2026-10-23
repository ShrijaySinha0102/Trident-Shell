#define _DEFAULT_SOURCE
#include "jobs.h"
#include "signals.h"
#include "stats.h"
#include "trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_JOBS 256
static Job g_jobs[MAX_JOBS];
extern int g_last_status;

Job *job_add(pid_t pgid, pid_t *pids, size_t n, const char *cmd, int bg) {
    sigset_t mask;
    signals_block_chld(&mask);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].id == 0) {
            Job *j = &g_jobs[i];
            j->id = i + 1;
            j->pgid = pgid;
            j->pids = malloc(sizeof(pid_t) * n);
            memcpy(j->pids, pids, sizeof(pid_t) * n);
            j->npids = n;
            j->state = JOB_RUNNING;
            j->cmdline = strdup(cmd);
            j->background = bg;
            clock_gettime(CLOCK_MONOTONIC, &j->start);
            trace_event("job_add id=%d pgid=%d cmd='%s'", j->id, (int)pgid, cmd);
            signals_unblock_chld(&mask);
            return j;
        }
    }
    signals_unblock_chld(&mask);
    return NULL;
}

Job *job_find_by_id(int id) {
    sigset_t mask;
    signals_block_chld(&mask);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].id == id) {
            signals_unblock_chld(&mask);
            return &g_jobs[i];
        }
    }
    signals_unblock_chld(&mask);
    return NULL;
}

Job *job_find_by_pgid(pid_t pgid) {
    sigset_t mask;
    signals_block_chld(&mask);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].id != 0 && g_jobs[i].pgid == pgid) {
            signals_unblock_chld(&mask);
            return &g_jobs[i];
        }
    }
    signals_unblock_chld(&mask);
    return NULL;
}

Job *job_find_by_pid(pid_t pid) {
    sigset_t mask;
    signals_block_chld(&mask);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].id != 0) {
            for (size_t k = 0; k < g_jobs[i].npids; k++) {
                if (g_jobs[i].pids[k] == pid) {
                    signals_unblock_chld(&mask);
                    return &g_jobs[i];
                }
            }
        }
    }
    signals_unblock_chld(&mask);
    return NULL;
}

void job_remove(Job *j) {
    if (!j) return;
    sigset_t mask;
    signals_block_chld(&mask);
    free(j->pids);
    free(j->cmdline);
    j->pids = NULL;
    j->cmdline = NULL;
    j->id = 0;
    signals_unblock_chld(&mask);
}

void jobs_print(void) {
    sigset_t mask;
    signals_block_chld(&mask);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].id != 0) {
            Job *j = &g_jobs[i];
            const char *st = (j->state == JOB_RUNNING) ? "Running" :
                             ((j->state == JOB_STOPPED) ? "Stopped" : "Done");
            printf("[%d] %-24s %s\n", j->id, st, j->cmdline);
        }
    }
    signals_unblock_chld(&mask);
}

int job_put_fg(Job *j, int cont) {
    if (!j) return 0;
    int tty = isatty(STDIN_FILENO);
    if (tty) {
        tcsetpgrp(STDIN_FILENO, j->pgid);
        trace_event("tcsetpgrp %d", (int)j->pgid);
    }
    if (cont) {
        kill(-j->pgid, SIGCONT);
        trace_event("sigcont pgid=%d", (int)j->pgid);
    }

    size_t n = j->npids;
    int last = 0, stopped = 0;
    for (size_t k = 0; k < n; k++) {
        if (j->pids[k] == 0) continue;
        int st = 0;
        struct rusage ru;
        pid_t r;
        do {
            r = wait4(j->pids[k], &st, WUNTRACED, &ru);
        } while (r < 0 && errno == EINTR);

        if (r > 0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double wall = (now.tv_sec - j->start.tv_sec) + (double)(now.tv_nsec - j->start.tv_nsec) / 1e9;
            stats_record(r, &ru, wall);
            trace_event("wait4 pid=%d status=%d", (int)r, st);

            if (WIFSTOPPED(st)) {
                stopped = 1;
                last = 128 + WSTOPSIG(st);
            } else {
                j->pids[k] = 0;
                if (k == n - 1) {
                    last = WIFEXITED(st) ? WEXITSTATUS(st) : (WIFSIGNALED(st) ? 128 + WTERMSIG(st) : 0);
                }
            }
        }
    }

    if (tty) tcsetpgrp(STDIN_FILENO, getpgrp());
    g_last_status = last;

    if (stopped) {
        j->state = JOB_STOPPED;
        j->background = 1;
        fprintf(stderr, "\n[%d] Stopped                 %s\n", j->id, j->cmdline);
    } else {
        if (last >= 128 && last != 127) {
            fprintf(stderr, "\n");
        }
        job_remove(j);
    }
    return last;
}

int job_put_bg(Job *j, int cont) {
    if (!j) return 0;
    if (cont) {
        kill(-j->pgid, SIGCONT);
        trace_event("sigcont pgid=%d", (int)j->pgid);
    }
    j->state = JOB_RUNNING;
    j->background = 1;
    fprintf(stderr, "[%d] %s &\n", j->id, j->cmdline);
    return 0;
}


