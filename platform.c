#include <stdint.h>

#define EPSL_COMMON_PREFIX "PPF_"

#define PPF_INVALID_INT INT64_MIN

int64_t PPF_platform_invalid_int_val(void) {
    return PPF_INVALID_INT;
}

#ifdef _WIN32

#else

#include <unistd.h>

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
