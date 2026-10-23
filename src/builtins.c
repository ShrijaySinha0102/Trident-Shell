#include "builtins.h"
#include "jobs.h"
#include "trace.h"
#include "stats.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int g_last_status;

static const char *names[] = { "cd", "exit", "export", "jobs", "fg", "bg",
                               "trace", "stats", NULL };

int is_builtin(const char *name) {
    if (!name) return 0;
    for (int i = 0; names[i]; i++)
        if (strcmp(name, names[i]) == 0) return 1;
    return 0;
}

int run_builtin(Command *c) {
    if (!c || !c->argv || !c->argv[0]) return 0;
    const char *name = c->argv[0];
    
    if (strcmp(name, "cd") == 0) {
        const char *dir = c->argv[1];
        if (!dir) {
            dir = getenv("HOME");
            if (!dir) {
                fprintf(stderr, "nsh: cd: HOME not set\n");
                return 1;
            }
        }
        if (chdir(dir) != 0) {
            perror("nsh: cd");
            return 1;
        }
        return 0;
    } else if (strcmp(name, "exit") == 0) {
        if (c->argv[1]) exit(atoi(c->argv[1]));
        exit(g_last_status);
    } else if (strcmp(name, "export") == 0) {
        if (c->argv[1]) {
            char *eq = strchr(c->argv[1], '=');
            if (eq) {
                *eq = '\0';
                setenv(c->argv[1], eq + 1, 1);
                *eq = '=';
            } else {
                setenv(c->argv[1], "", 1);
            }
        }
        return 0;
    } else if (strcmp(name, "jobs") == 0) {
        jobs_print();
        return 0;
    } else if (strcmp(name, "fg") == 0) {
        Job *j = NULL;
        if (c->argv[1]) {
            const char *arg = c->argv[1];
            if (arg[0] == '%') arg++;
            int id = atoi(arg);
            j = job_find_by_id(id);
        } else {
            for (int i = 256; i >= 1; i--) {
                Job *cand = job_find_by_id(i);
                if (cand) { j = cand; break; }
            }
        }
        if (!j) {
            fprintf(stderr, "nsh: fg: no such job\n");
            return 1;
        }
        fprintf(stderr, "%s\n", j->cmdline);
        return job_put_fg(j, 1);
    } else if (strcmp(name, "bg") == 0) {
        Job *j = NULL;
        if (c->argv[1]) {
            const char *arg = c->argv[1];
            if (arg[0] == '%') arg++;
            int id = atoi(arg);
            j = job_find_by_id(id);
        } else {
            for (int i = 256; i >= 1; i--) {
                Job *cand = job_find_by_id(i);
                if (cand && cand->state == JOB_STOPPED) { j = cand; break; }
            }
            if (!j) {
                for (int i = 256; i >= 1; i--) {
                    Job *cand = job_find_by_id(i);
                    if (cand) { j = cand; break; }
                }
            }
        }
        if (!j) {
            fprintf(stderr, "nsh: bg: no such job\n");
            return 1;
        }
        return job_put_bg(j, 1);
    } else if (strcmp(name, "trace") == 0) {
        if (c->argv[1] && strcmp(c->argv[1], "on") == 0) {
            g_trace = 1;
            return 0;
        } else if (c->argv[1] && strcmp(c->argv[1], "off") == 0) {
            g_trace = 0;
            return 0;
        } else {
            fprintf(stderr, "usage: trace on|off\n");
            return 1;
        }
    } else if (strcmp(name, "stats") == 0) {
        stats_print();
        return 0;
    }
    return 0;
}
