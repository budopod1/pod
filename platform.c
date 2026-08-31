#define _POSIX_C_SOURCE 202405L

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "platform.h"

#include "epsilon.h"

#define dup_Cstr_to_Estr(a, b) ((ARRAY_Byte*)epsl_dup_Cstr_to_Estr(a, b))

#define PPF_INVALID_INT INT64_MIN

int64_t PPF_platform_invalid_int_val(void) {
    return PPF_INVALID_INT;
}

#ifdef _WIN32

#error "Windows is not supported"

#else

#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <glob.h>

#endif

int64_t PPF_current_uid(void) {
#ifdef _WIN32
    return PPF_INVALID_INT;
#else
    return getuid();
#endif
}

int64_t PPF_current_euid(void) {
#ifdef _WIN32
    return PPF_INVALID_INT;
#else
    return geteuid();
#endif
}

ARRAY_ARRAY_Byte *PPF_glob_paths(ARRAY_ARRAY_Byte *patterns) {
#ifdef _WIN32
    epsl_panicf("glob not supported on windows")
#else
    glob_t globbuf = {0};

    for (uint64_t i = 0; i < patterns->length; i++) {
        ARRAY_Byte *epsl_pattern = patterns->content[i];
        EPSL_STR_TO_C_STR(epsl_pattern, c_pattern);
        int flags = i ? GLOB_APPEND : 0;
        int status = glob(c_pattern, flags, NULL, &globbuf);
        if (status != 0 && status != GLOB_NOMATCH) {
            epsl_panicf("glob returned unexpected status: %d", status);
        }
        CLEANUP_CONV_C_STR(c_pattern);
    }

    ARRAY_ARRAY_Byte *matches = epsl_malloc(sizeof(*matches));
    matches->ref_counter = 0;
    uint64_t length = globbuf.gl_pathc;
    matches->length = length;
    uint64_t capacity = length == 0 ? 1 : length;
    matches->capacity = capacity;
    matches->content = epsl_malloc(capacity * sizeof(ARRAY_Byte*));

    for (uint64_t i = 0; i < length; i++) {
        matches->content[i] = dup_Cstr_to_Estr(1, globbuf.gl_pathv[i]);
    }

    globfree(&globbuf);

    return matches;
#endif
}

bool PPF_fork_proc(void) {
#ifdef _WIN32
    epsl_panicf("fork not supported on windows")
#else
    pid_t pid = fork();
    if (pid == -1) {
        epsl_panicf("fork failed: %s", strerror(errno));
    } else if (pid == 0) {
        return true;
    } else {
        return false;
    }
#endif
}

PipePair *PPF_make_pipe_pair(void) {
    int ends[2];
    int status = pipe(ends);
    if (status) {
        epsl_panicf("failed to create pipe: %s", strerror(errno));
    }
    PipePair *pair = malloc(sizeof(pair));
    pair->ref_counter = 0;
    pair->in = ends[0];
    pair->out = ends[1];
    return pair;
}

int64_t PPF_swap_std_fd(int64_t target, int64_t replacement) {
    if (target == 0) {
        fflush(stdin);
    } else if (target == 1) {
        fflush(stdout);
    } else if (target == 2) {
        fflush(stderr);
    }
    int current = dup((int)target);
    if (current == -1) {
        epsl_panicf("failed to dup file descriptor %d: %s",
            (int)target, strerror(errno));
    }
    if (dup2((int)replacement, (int)target) == -1) {
        epsl_panicf("failed to dup2 fd %d to %d: %s",
            (int)replacement, (int)target, strerror(errno));
    }
    close((int)replacement);
    return current;
}

void PPF_close_fd(int64_t fd) {
    close((int)fd);
}

void PPF_restore_std_fd(int64_t target, int64_t original) {
    fflush(stdout);
    if (dup2((int)original, (int)target) == -1) {
        epsl_panicf("failed to dup2 original fd %d to %d: %s",
            (int)original, (int)target, strerror(errno));
    }
    close((int)original);
}

bool is_sigint_capatured = false;

sig_atomic_t interrupt_requested = 0;

static void sigint_handler(int s) {
    interrupt_requested = 1;
}

void PPF_set_sigint_captured(bool capture) {
    is_sigint_capatured = capture;
    restore_sigint_handler();
}

void restore_sigint_handler(void) {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    if (is_sigint_capatured) {
        act.sa_handler = &sigint_handler;
        act.sa_flags = SA_RESTART;
    } else {
        act.sa_handler = SIG_IGN;
        act.sa_flags = 0;
    }
    sigaction(SIGINT, &act, NULL);
}

bool PPF_consume_interrupt_request(void) {
    if (interrupt_requested) {
        interrupt_requested = 0;
        return true;
    }
    return false;
}
