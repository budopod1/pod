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

#warning "Windows is not fully supported"

#else

#include <unistd.h>
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
