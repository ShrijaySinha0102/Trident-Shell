#ifndef SIGNALS_H
#define SIGNALS_H
#include <signal.h>
extern volatile sig_atomic_t g_sigchld_pending;
void signals_init(void);
void signals_reset_child(void);
void signals_block_chld(sigset_t *prev);
void signals_unblock_chld(const sigset_t *prev);
#endif
