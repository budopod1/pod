#include <stdbool.h>
#include <stdint.h>

#define EPSL_COMMON_PREFIX "PPF_"
#define EPSL_IMPLEMENTATION_LOCATION "platform.c"

typedef struct ARRAY_Byte {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    unsigned char *content;
} ARRAY_Byte;

typedef struct ARRAY_ARRAY_Byte {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    struct ARRAY_Byte **content;
} ARRAY_ARRAY_Byte, ARRAY_ARRAY_Byte;

typedef struct PipePair {
    uint64_t ref_counter;
    int64_t in;
    int64_t out;
} PipePair;

int64_t PPF_platform_invalid_int_val(void);

int64_t PPF_current_uid(void);

int64_t PPF_current_euid(void);

ARRAY_ARRAY_Byte *PPF_glob_paths(ARRAY_ARRAY_Byte *patterns);

bool PPF_fork_proc(void);

PipePair *PPF_make_pipe_pair(void);

int64_t PPF_swap_std_fd(int64_t target, int64_t replacement);

void PPF_close_fd(int64_t fd);

void PPF_restore_std_fd(int64_t target, int64_t original);
