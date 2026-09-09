#include <stdint.h>

#define EPSL_COMMON_PREFIX "HSH_"
#define EPSL_IMPLEMENTATION_LOCATION "hash.c"

struct PolymorphicStruct {
    void *struct_;
    void *vtable;
};

typedef struct ARRAY_Byte {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    unsigned char *content;
} ARRAY_Byte;

uint64_t HSH_hash_uint64(uint64_t x);

uint64_t HSH_hash_int64(int64_t x);

uint64_t HSH_hash_double(double x);

uint64_t HSH_hash_struct_ptr(struct PolymorphicStruct struct_);

uint64_t HSH_hash_string(ARRAY_Byte *str);
