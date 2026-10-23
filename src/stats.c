#include "stats.h"
#include <stdio.h>

#define MAX_STATS 256

typedef struct {
    pid_t pid;
    double wall_sec;
    double user_sec;
    double sys_sec;
    long maxrss_kb;
} StatEntry;

static StatEntry g_stats[MAX_STATS];
static size_t g_stat_count = 0;

void stats_record(pid_t pid, const struct rusage *ru, double wall_sec) {
    if (g_stat_count >= MAX_STATS) return;
    StatEntry *e = &g_stats[g_stat_count++];
    e->pid = pid;
    e->wall_sec = wall_sec;
    if (ru) {
        e->user_sec = ru->ru_utime.tv_sec + (double)ru->ru_utime.tv_usec / 1e6;
        e->sys_sec = ru->ru_stime.tv_sec + (double)ru->ru_stime.tv_usec / 1e6;
        e->maxrss_kb = ru->ru_maxrss;
    } else {
        e->user_sec = 0.0;
        e->sys_sec = 0.0;
        e->maxrss_kb = 0;
    }
}

void stats_print(void) {
    if (g_stat_count == 0) {
        printf("No process stats recorded yet.\n");
        return;
    }
    printf("%-8s %10s %10s %10s %12s\n", "PID", "WALL(s)", "USER(s)", "SYS(s)", "MAXRSS(KB)");
    for (size_t i = 0; i < g_stat_count; i++) {
        StatEntry *e = &g_stats[i];
        printf("%-8d %10.4f %10.4f %10.4f %12ld\n",
               (int)e->pid, e->wall_sec, e->user_sec, e->sys_sec, e->maxrss_kb);
    }
}

