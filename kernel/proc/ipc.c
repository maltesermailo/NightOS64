//
// Created by Jannik on 11.08.2024.
//
#include "process.h"
#include "message.h"
#include "server.h"
#include <signal.h>
#include <string.h>

//Sigset
// Initialize a signal set to empty
inline int sigemptyset(sigset_t *set) {
    for (int i = 0; i < SIGSET_NWORDS; i++) {
        set->sig[i] = 0;
    }
    return 0;
}

// Initialize a signal set to full (all signals)
inline int sigfillset(sigset_t *set) {
    for (int i = 0; i < SIGSET_NWORDS; i++) {
        set->sig[i] = ~0;
    }

    return 0;
}

// Add a signal to a signal set
inline int sigaddset(sigset_t *set, int signum) {
    if (signum <= 0 || signum > 1024) return -1;
    set->sig[(signum-1)/(8*sizeof(uint32_t))] |= 1UL << ((signum-1)%(8*sizeof(uint32_t)));
    return 0;
}

// Remove a signal from a signal set
inline int sigdelset(sigset_t *set, int signum) {
    if (signum <= 0 || signum > 1024) return -1;
    set->sig[(signum-1)/(8*sizeof(uint32_t))] &= ~(1UL << ((signum-1)%(8*sizeof(uint32_t))));
    return 0;
}

// Check if a signal is in a signal set
inline int sigismember(const sigset_t *set, int signum) {
    if (signum <= 0 || signum > 1024) return -1;
    return (set->sig[(signum-1)/(8*sizeof(uint32_t))] & (1UL << ((signum-1)%(8*sizeof(uint32_t))))) != 0;
}

int has_pending_signals(struct process *p) {
    for (int i = 0; i < SIGSET_NWORDS; i++) {
        if (p->pending_signals.sig[i] != 0) {
            return 1;  // There are pending signals
        }
    }
    return 0;  // No pending signals
}

void process_set_signal_mask(int how, sigset_t* new) {
    process_t* current = get_current_process();

    if(how == SIG_BLOCK) {
        for (int i = 0; i < SIGSET_NWORDS; i++) {
            current->blocked_signals.sig[i] = new->sig[i] | current->blocked_signals.sig[i];
        }
    }

    if(how == SIG_UNBLOCK) {
        for (int i = 0; i < SIGSET_NWORDS; i++) {
            if(sigismember(new, i)) {
                sigdelset(&current->blocked_signals, i);
            }
        }
    }

    if(how == SIG_SETMASK) {
        for (int i = 0; i < SIGSET_NWORDS; i++) {
            current->blocked_signals.sig[i] = current->blocked_signals.sig[i];
        }
    }
}

void process_set_signal_handler(int signum, struct sigaction* action) {
    process_t* proc = get_current_process();

    spin_lock(&proc->lock);
    proc->signalHandlers[signum].handler = action->sa_handler;
    proc->signalHandlers[signum].sa_mask = action->sa_mask;
    proc->signalHandlers[signum].sa_flags = action->sa_flags;

    if(action->sa_flags & 0x04000000) {
        proc->signalHandlers[signum].sa_restorer = action->sa_restorer;
    }

    spin_unlock(&proc->lock);
}

signal_handler_t* process_get_signal_handler(int signum) {
    process_t* proc = get_current_process();

    return &proc->signalHandlers[signum];
}

void handle_signal(process_t* process, int signum, regs_t* regs) {
    if(!process->signalHandlers[signum].handler) {
        switch(signum) {
            //Terminate
            case SIGALRM:
            case SIGKILL:
            case SIGHUP:
            case SIGINT:
            case SIGIO:
            case SIGPIPE:
            case SIGTERM:
            case SIGUSR1:
            case SIGUSR2:
                process_exit(0);
                break;
            //Core dump
            case SIGABRT:
            case SIGFPE:
            case SIGILL:
            case SIGQUIT:
            case SIGSEGV:
                process_exit(0);
                break;
            //STOP
            case SIGSTOP:
            case SIGTSTP:
            case SIGTTIN:
            case SIGTTOU: {
                __sync_or_and_fetch(&process->flags, PROC_FLAG_SLEEP_INTERRUPTIBLE);
                __sync_and_and_fetch(&process->flags, ~(PROC_FLAG_ON_CPU));

                while(!has_pending_signals(process)) {
                    schedule(true);
                }
                break;
            }
            //CONT
            case SIGCONT:
                break;
            default:
                break;
        }
    }

    if(process->signalHandlers[signum].handler == SIG_DFL) {
        if(signum == SIGKILL) {
            process_exit(0);
            __builtin_unreachable();
        }

        return;
    }

    if(get_current_process()->signalHandlers[signum].sa_flags & SA_NODEFER) {
        sigaddset(&get_current_process()->blocked_signals, signum);
    }

    //SETUP SIGNAL HANDLER
    process_enter_signal(regs, signum);
}

void process_check_signals(regs_t* regs) {
    process_t* current = get_current_process();

    if(current->flags & PROC_FLAG_FINISHED) {
        return;
    }

    uintptr_t rflags = cli();
    spin_lock(&current->lock);

    for (int i = 1; i < 32; i++) {
        if (sigismember(&current->pending_signals, i) && !sigismember(&current->blocked_signals, i)) {
            sigdelset(&current->pending_signals, i);
            spin_unlock(&current->lock);
            sti(rflags);
            handle_signal(current, i, regs);
        }

        spin_unlock(&current->lock);
        sti(rflags);
    }
}