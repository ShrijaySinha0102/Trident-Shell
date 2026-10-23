#include "signals.h"
#include <signal.h>
#include <stddef.h>

volatile sig_atomic_t g_sigchld_pending = 0;

static void sigchld_handler(int sig) {
    (void)sig;
    g_sigchld_pending = 1;
}

static void set_handler(int sig, void (*handler)(int), int flags) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = flags;
    sigaction(sig, &sa, NULL);
}

void signals_init(void) {
    set_handler(SIGCHLD, sigchld_handler, SA_RESTART);
    set_handler(SIGINT, SIG_IGN, 0);
    set_handler(SIGTSTP, SIG_IGN, 0);
    set_handler(SIGTTOU, SIG_IGN, 0);
    set_handler(SIGTTIN, SIG_IGN, 0);
}

void signals_reset_child(void) {
    set_handler(SIGINT, SIG_DFL, 0);
    set_handler(SIGTSTP, SIG_DFL, 0);
    set_handler(SIGTTOU, SIG_DFL, 0);
    set_handler(SIGTTIN, SIG_DFL, 0);
    set_handler(SIGCHLD, SIG_DFL, 0);
}

void signals_block_chld(sigset_t *prev) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, prev);
}

void signals_unblock_chld(const sigset_t *prev) {
    sigprocmask(SIG_SETMASK, prev, NULL);
}
