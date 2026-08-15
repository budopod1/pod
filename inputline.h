#ifndef INPUTLINE_H
#define INPUTLINE_H

#include <stdint.h>

#define USE_GNU_READLINE

#define EPSL_COMMON_PREFIX "IL_"
#define EPSL_IMPLEMENTATION_LOCATION "inputline.c"

typedef struct ARRAY_char {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    unsigned char *content;
} ARRAY_char, NULLABLE_ARRAY_char;

ARRAY_char *IL_inputline(ARRAY_char *prompt);

#endif
