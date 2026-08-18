#ifndef SUBPROC_H
#define SUBPROC_H

#ifdef _MSC_VER
#error "Windows not supported
#endif

#include <stdbool.h>
#include <stdint.h>

#define EPSL_COMMON_PREFIX "SPR_"
#define EPSL_IMPLEMENTATION_LOCATION "subproc.c"

typedef struct ARRAY_Byte {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    unsigned char *content;
} ARRAY_Byte, NULLABLE_ARRAY_Byte;

typedef struct ARRAY_ARRAY_Byte {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    struct ARRAY_Byte **content;
} ARRAY_ARRAY_Byte;

typedef struct ProcEnvVal {
    uint64_t ref_counter;
    ARRAY_Byte *name;
    ARRAY_Byte *val;
} ProcEnvVal;

typedef struct ARRAY_ProcEnvVal {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    ProcEnvVal **content;
} ARRAY_ProcEnvVal;

/*
output redirect mode key:
0 - do not redirect
1 - redirect to stdout (only for stderr)
2 - redirect to stderr (only for stdout)
3 - redirect to specified file
4 - capture
*/

typedef struct ProcInitInfo {
    uint64_t ref_counter;

    uint32_t stdout_mode;
    NULLABLE_ARRAY_Byte *stdout_file;

    uint32_t stderr_mode;
    NULLABLE_ARRAY_Byte *stderr_file;

    ARRAY_ProcEnvVal *env_vals;
    struct ARRAY_Byte *program;
    struct ARRAY_ARRAY_Byte *args;
} ProcInitInfo;

typedef struct Process {
    uint64_t ref_counter;
    struct ARRAY_Byte *program;
    uint32_t pid;
    bool completed;
    int32_t result_status;
} Process, NULLABLE_Process;

typedef struct ProcError {
    uint64_t ref_counter;
    ARRAY_Byte *msg;
} ProcError, NULLABLE_ProcError;

typedef struct ProcessResult {
    uint64_t ref_counter;
    NULLABLE_Process *maybe_process;
    NULLABLE_ProcError *maybe_error;
} ProcessResult;

ProcessResult *SPR_start_proc(ProcInitInfo *info);

NULLABLE_ProcError *SPR_await_proc_completion(Process *process);

NULLABLE_ProcError *SPR_poll_proc_status(Process *process);

NULLABLE_ProcError *SPR_send_proc_signal(Process *process, ARRAY_Byte *signame);

#endif
