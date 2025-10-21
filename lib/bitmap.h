#ifndef BITMAP_H
#define BITMAP_H

#include <cstdint>

typedef int64_t bitmap;

inline void bitmap_set(bitmap* bm, int64_t i)
{
    (*bm) = (*bm)|(1<<i);
}

inline bool bitmap_get(const bitmap* bm, int64_t i)
{
    return (*bm)&(1<<i);
}

inline void bitmap_flip(bitmap* bm, int64_t i)
{
    (*bm) = (*bm)^(1<<i);
}

inline bool bitmap_and(const bitmap* bm, int64_t beg, int64_t end)
{
    for (auto i = beg; i < end; i++) {
        if (!bitmap_get(bm, i)) {
            return false;
        }
    }
    return true;
}

#endif