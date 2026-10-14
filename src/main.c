#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "expand.h"
#include "parser.h"
#include "executor.h"
#include "signals.h"
#include "jobs.h"
#include <sys/wait.h>
#include <unistd.h>

/* REPL skeleton. Currently every line is a "not implemented" stub. */
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
            while ((p = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
                Job *j = job_find_by_pgid(p);
                if (!j) continue;
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    j->state = JOB_DONE;
                    fprintf(stderr, "[%d] Done                    %s\n", j->id, j->cmdline);
                    job_remove(j);
                } else if (WIFSTOPPED(status)) {
                    j->state = JOB_STOPPED;
                    fprintf(stderr, "\n[%d] Stopped                 %s\n", j->id, j->cmdline);
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
        if (lex(line, &toks) < 0) { continue; }
        if (expand_tokens(&toks) == 0 && parse(&toks, &pl) == 0) {
            pl.raw = strdup(line);
            g_last_status = execute(&pl);
            pipeline_free(&pl);
        }
        tokenlist_free(&toks);
    }
    free(line);
    return g_last_status;
}
