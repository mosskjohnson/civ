#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define CLAMP(x, lo, hi) (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))

void mem_or(const void* restrict src, void* restrict dest, size_t bytes_size);


#endif
