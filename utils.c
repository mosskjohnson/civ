#include "utils.h"

void mem_or(const void* restrict src, void* restrict dest, size_t bytes_size) {
    const unsigned char* src_bytes = (const unsigned char*)src;
    unsigned char* dest_bytes = (unsigned char*)dest;
    for (size_t i = 0; i < bytes_size; ++i) {
        dest_bytes[i] |= src_bytes[i];
    }
}
