#define _POSIX_C_SOURCE 202405L

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32

#error "Windows not supported

#else

#include <poll.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#endif

#include "epsilon.h"
#include "subproc.h"

#if __STDC_VERSION__ < 202311L
#define noreturn _Noreturn
#endif

static ARRAY_Byte *C_str_to_epsl_str(uint64_t ref_counter, char *src) {
    ARRAY_Byte *result = epsl_malloc(sizeof(*result));
    result->ref_counter = ref_counter;
    uint64_t length = strlen(src);
    result->capacity = length + 1;
    result->length = length;
    result->content = (unsigned char*)src;
    return result;
}

static char *epsl_str_to_C_str(struct ARRAY_Byte *str) {
    char *result = epsl_malloc(str->length + 1);
    memcpy(result, str->content, str->length);
    result[str->length] = '\0';
    return result;
}

#define EPSL_STR_TO_C_STR(epsl_str, str_name)\
    char *str_name;\
    bool str_name##_is_new_str = epsl_str->capacity <= epsl_str->length;\
    if (str_name##_is_new_str) {\
        str_name = epsl_malloc(epsl_str->length+1);\
        memcpy(str_name, epsl_str->content, epsl_str->length);\
    } else {\
        str_name = (char*)epsl_str->content;\
    }\
    str_name[epsl_str->length] = '\0';

#define CLEANUP_C_STR(str_name)\
    if (str_name##_is_new_str) free(str_name);

static ProcessResult *result_error(char *msg) {
    ProcError *error = epsl_malloc(sizeof(*error));
    error->ref_counter = 1;
    error->msg = C_str_to_epsl_str(1, msg);

    ProcessResult *result = epsl_calloc(1, sizeof(*result));
    result->maybe_error = error;
    return result;
}

static ProcessResult *result_errorf(const char *format, ...) {
    va_list vargs;
    va_start(vargs, format);
    size_t msg_len = vsnprintf(NULL, 0, format, vargs);
    char *buffer = epsl_malloc(msg_len + 1);
    vsprintf(buffer, format, vargs);
    ProcessResult *result = result_error(buffer);
    va_end(vargs);
}

static void subproc_set_env(ARRAY_ProcEnvVal *env_vals) {
    for (uint64_t i = 0; i < env_vals->length; i++) {
        ProcEnvVal *env_val = env_vals->content[i];
        EPSL_STR_TO_C_STR(env_val->name, name);
        EPSL_STR_TO_C_STR(env_val->val, val);
        setenv(name, val, 1);
        CLEANUP_C_STR(name);
    }
}

noreturn static void subproc_run(ProcInitInfo *info) {
    subproc_set_env(info->env_vals);

    EPSL_STR_TO_C_STR(info->program, cmd);

    char **args_buffer = epsl_malloc(
        sizeof(char*) * (info->args->length + 1)
    );

    for (uint64_t i = 0; i < info->args->length; i++) {
        args_buffer[i] = epsl_str_to_C_str(info->args->content[i]);
    }

    args_buffer[info->args->length] = NULL;

    execvp(cmd, args_buffer);

    fprintf(stderr, "Failed to start subprocess %s\n", cmd);
    exit(1);
}

ProcessResult *SPR_start_proc(ProcInitInfo *info) {
    pid_t pid = fork();

    if (pid < 0) {
        return result_errorf("Failed to start subprocess");
    } else if (pid == 0) {
        subproc_run(info);
    }

    Process *process = epsl_malloc(sizeof(*process));
    process->ref_counter = 1;
    process->pid = pid;

    ProcessResult *result = epsl_calloc(1, sizeof(*result));
    result->maybe_process = process;
    return result;
}

NULLABLE_ProcError *SPR_await_proc_completion(Process *process) {
    while (true) {
        int wstatus;
        if (waitpid(process->pid, &wstatus, 0) < 0) {
            return result_errorf("Error while waiting for process completion");
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
