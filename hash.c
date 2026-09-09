#include <string.h>

#include "hash.h"

#define lengthof(x) (sizeof(x) / sizeof(*(x)))

uint64_t HSH_hash_uint64(uint64_t x) {
    // https://stackoverflow.com/a/12996028
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

uint64_t HSH_hash_int64(int64_t x) {
    return HSH_hash_uint64((uint64_t)x);
}

uint64_t HSH_hash_double(double x) {
    union {
        double f64;
        uint64_t u64;
    } c;
    c.f64 = x;
    return HSH_hash_uint64((uint64_t)c.u64);
}

uint64_t HSH_hash_struct_ptr(struct PolymorphicStruct struct_) {
    return HSH_hash_uint64((uint64_t)struct_.struct_);
}

static uint32_t PRIMES[] = {2, 3, 5, 7, 11, 13, 17, 19};
static uint32_t PRIME_COUNT = lengthof(PRIMES);

uint64_t HSH_hash_string(ARRAY_Byte *str) {
    uint64_t full_hash = 0;
    for (uint64_t i = 0; i < str->length; i += 8) {
        union {
            char bytes[8];
            uint64_t u64;
        } c;
        c.u64 = 0;
        uint64_t l = str->length - i;
        if (l > 8) l = 8;
        memcpy(c.bytes, str->content + i, l);
        uint64_t part_hash = HSH_hash_uint64(c.u64);
        uint64_t multiplier = PRIMES[(i / 8) % PRIME_COUNT];
        full_hash ^= part_hash * multiplier;
    } 
    return full_hash;
}
