#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "expand.h"
#include "parser.h"
#include "executor.h"
#include "signals.h"
#include "jobs.h"
#include "trace.h"
#include "stats.h"
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>

int main(void) {
    char *line = NULL;
    size_t cap = 0;

    signals_init();
    int interactive = isatty(STDIN_FILENO);
    if (interactive) {
        setpgid(0, 0);
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }

    for (;;) {
        if (g_sigchld_pending) {
            g_sigchld_pending = 0;
            int status;
            pid_t p;
            struct rusage ru;
            while ((p = wait4(-1, &status, WNOHANG | WUNTRACED, &ru)) > 0) {
                Job *j = job_find_by_pid(p);
                if (!j) continue;
                stats_record(p, &ru, 0.0);
                trace_event("wait4 pid=%d status=%d", (int)p, status);
                if (WIFSTOPPED(status)) {
                    j->state = JOB_STOPPED;
                    fprintf(stderr, "\n[%d] Stopped                 %s\n", j->id, j->cmdline);
                } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    size_t remaining = 0;
                    for (size_t k = 0; k < j->npids; k++) {
                        if (j->pids[k] == p) j->pids[k] = 0;
                        if (j->pids[k] != 0) remaining++;
                    }
                    if (remaining == 0) {
                        j->state = JOB_DONE;
                        fprintf(stderr, "[%d] Done                    %s\n", j->id, j->cmdline);
                        job_remove(j);
                    }
                }
            }
        }

        if (interactive) {
            fputs("nsh> ", stdout);
            fflush(stdout);
        }
        if (getline(&line, &cap, stdin) < 0) {
            if (interactive) putchar('\n');
            break;
        }
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;

        TokenList toks;
        Pipeline  pl;
        if (lex(line, &toks) < 0) { g_last_status = 2; continue; }
        if (expand_tokens(&toks) == 0 && parse(&toks, &pl) == 0) {
            pl.raw = strdup(line);
            g_last_status = execute(&pl);
            pipeline_free(&pl);
        } else {
            g_last_status = 2;   /* syntax error: same status as bash */
        }
        tokenlist_free(&toks);
    }
    free(line);
    return g_last_status;
}