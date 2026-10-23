#ifndef JOBS_H
#define JOBS_H
#include <stddef.h>
#include <sys/types.h>
#include <time.h>
typedef enum { JOB_RUNNING, JOB_STOPPED, JOB_DONE } JobState;
typedef struct {
    int      id;
    pid_t    pgid;
    pid_t   *pids; size_t npids;
    JobState state;
    char    *cmdline;
    int      background;
    struct timespec start;
} Job;
Job *job_add(pid_t pgid, pid_t *pids, size_t n, const char *cmd, int bg);
Job *job_find_by_id(int id);
Job *job_find_by_pgid(pid_t pgid);
Job *job_find_by_pid(pid_t pid);
void job_remove(Job *j);
void jobs_print(void);
int  job_put_fg(Job *j, int cont);
int  job_put_bg(Job *j, int cont);
#endif
