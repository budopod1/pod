#define _POSIX_C_SOURCE 202405L

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32

#error "Windows not supported

#else

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

#endif

#include "epsilon.h"
#include "subproc.h"

#if __STDC_VERSION__ < 202311L
#define noreturn _Noreturn
#endif

#define Cstr_to_Estr(a, b) ((ARRAY_Byte*)epsl_Cstr_to_Estr(a, b))

#define Estr_to_Cstr(a) epsl_Estr_to_Cstr((struct Array*)(a))

ProcError *proc_errorf(const char *format, ...) {
    va_list vargs1;
    va_start(vargs1, format);
    va_list vargs2;
    va_copy(vargs2, vargs1);
    
    size_t msg_len = vsnprintf(NULL, 0, format, vargs1);
    char *buffer = epsl_malloc(msg_len + 1);
    vsprintf(buffer, format, vargs2);

    va_end(vargs1);
    va_end(vargs2);

    ProcError *error = epsl_malloc(sizeof(*error));
    error->ref_counter = 0;
    error->msg = Cstr_to_Estr(1, buffer);
    return error;
}

static ProcessResult *result_error(struct ProcError *error) {
    ProcessResult *result = epsl_calloc(1, sizeof(*result));
    error->ref_counter++;
    result->maybe_error = error;
    return result;
}

static void subproc_set_env(ARRAY_ProcEnvVal *env_vals) {
    for (uint64_t i = 0; i < env_vals->length; i++) {
        ProcEnvVal *env_val = env_vals->content[i];
        EPSL_STR_TO_C_STR(env_val->name, name);
        EPSL_STR_TO_C_STR(env_val->val, val);
        setenv(name, val, 1);
        CLEANUP_CONV_C_STR(name);
        CLEANUP_CONV_C_STR(val);
    }
}

noreturn static void subproc_run(ProcInitInfo *info) {
    subproc_set_env(info->env_vals);

    EPSL_STR_TO_C_STR(info->program, cmd);

    char **args_buffer = epsl_malloc(
        sizeof(char*) * (info->args->length + 1)
    );

    for (uint64_t i = 0; i < info->args->length; i++) {
        args_buffer[i] = Estr_to_Cstr(info->args->content[i]);
    }

    args_buffer[info->args->length] = NULL;

    execvp(cmd, args_buffer);

    fprintf(stderr, "Failed to start subprocess %s\n", cmd);
    exit(1);
}

ProcessResult *SPR_start_proc(ProcInitInfo *info) {
    pid_t pid = fork();

    if (pid < 0) {
        return result_error(proc_errorf(
            "Failed to start subprocess: %s", strerror(errno)
        ));
    } else if (pid == 0) {
        subproc_run(info);
    }

    Process *process = epsl_malloc(sizeof(*process));
    process->ref_counter = 1;
    process->program = info->program;
    process->program->ref_counter++;
    process->pid = pid;
    process->completed = false;
    process->result_status = -1;

    ProcessResult *result = epsl_calloc(1, sizeof(*result));
    result->maybe_process = process;
    return result;
}

static NULLABLE_ProcError *proc_waitpid(Process *process, int options) {
    while (true) {
        int wstatus;
        pid_t s = waitpid(process->pid, &wstatus, options);
        if (s < 0) {
            return proc_errorf("Failed to get process status: %s", strerror(errno));
        } else if (s == 0) {
            return NULL;
        }
        if (WIFEXITED(wstatus)) {
            process->result_status = WEXITSTATUS(wstatus);
            break;
        } else if (WIFSIGNALED(wstatus)) {
            process->result_status = WTERMSIG(wstatus) + 128;
            break;
        }
    }
    process->completed = true;
    return NULL;
} 

NULLABLE_ProcError *SPR_await_proc_completion(Process *process) {
    return proc_waitpid(process, 0);
}

NULLABLE_ProcError *SPR_poll_proc_status(Process *process) {
    return proc_waitpid(process, WNOHANG);
}

struct SignalNamePair {
    const char *name;
    int signal;
};

#define SIGNAL_NAME_PAIR(name) {#name, name}

const struct SignalNamePair signal_names[] = {
    SIGNAL_NAME_PAIR(SIGHUP),
    SIGNAL_NAME_PAIR(SIGINT),
    SIGNAL_NAME_PAIR(SIGQUIT),
    SIGNAL_NAME_PAIR(SIGILL),
    SIGNAL_NAME_PAIR(SIGTRAP),
    SIGNAL_NAME_PAIR(SIGABRT),
    SIGNAL_NAME_PAIR(SIGFPE),
    SIGNAL_NAME_PAIR(SIGKILL),
    SIGNAL_NAME_PAIR(SIGBUS),
    SIGNAL_NAME_PAIR(SIGSEGV),
    SIGNAL_NAME_PAIR(SIGSYS),
    SIGNAL_NAME_PAIR(SIGPIPE),
    SIGNAL_NAME_PAIR(SIGALRM),
    SIGNAL_NAME_PAIR(SIGTERM),
    SIGNAL_NAME_PAIR(SIGURG),
    SIGNAL_NAME_PAIR(SIGSTOP),
    SIGNAL_NAME_PAIR(SIGTSTP),
    SIGNAL_NAME_PAIR(SIGCONT),
    SIGNAL_NAME_PAIR(SIGCHLD),
    SIGNAL_NAME_PAIR(SIGTTIN),
    SIGNAL_NAME_PAIR(SIGTTOU),
    SIGNAL_NAME_PAIR(SIGIO),
    SIGNAL_NAME_PAIR(SIGXCPU),
    SIGNAL_NAME_PAIR(SIGXFSZ),
    SIGNAL_NAME_PAIR(SIGVTALRM),
    SIGNAL_NAME_PAIR(SIGPROF),
    SIGNAL_NAME_PAIR(SIGWINCH),
    SIGNAL_NAME_PAIR(SIGUSR1),
    SIGNAL_NAME_PAIR(SIGUSR2),
};

static int lookup_signal_number(char *name) {
    for (int i = 0; i < sizeof(signal_names) / sizeof(*signal_names); i++) {
        struct SignalNamePair pair = signal_names[i];
        if (strcmp(pair.name, name) == 0) {
            return pair.signal;
        }
    }
    return -1;
}

NULLABLE_ProcError *SPR_send_proc_signal(Process *process, ARRAY_Byte *signame) {
    EPSL_STR_TO_C_STR(signame, c_signame)
    int signal_number = lookup_signal_number(c_signame);
    CLEANUP_CONV_C_STR(c_signame);
    if (signal_number == -1) {
        return proc_errorf("Cannot find signal by specified name");
    }
    if (kill(process->pid, signal_number)) {
        return proc_errorf("Failed to send process signal: %s", strerror(errno));
    }
    return NULL;
}
