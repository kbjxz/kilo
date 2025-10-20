#ifndef SLICE_H
#define SLICE_H

#include "arena.h"
#include <cstddef>
#ifndef NDEBUG
#include <iostream>
#endif

template <typename T>
struct slice {
    T* data;
    size_t len;
    size_t cap;

    T& operator[](size_t i) 
    {
        assert(i < this->len, "out of bound! len:%d, i:%d", this->len, i);
        return this->data[i];
    }
};

template <typename T, typename A>
requires is_arena_allocator<A>
void slice_reserve(slice<T>* s, size_t new_cap, A* a)
{
    if (s->cap >= new_cap) {
        return;
    }
    T* new_data = arena_alloc<T>(a, new_cap);
    if (s->data) {
        memcpy(new_data, s->data, s->len);
    }
    s->data = new_data;
    s->cap = new_cap;
}

template <typename T, typename A>
requires is_arena_allocator<A>
void slice_make_n(slice<T>* s, size_t len, A* a)
{
    s->data = arena_alloc<T>(a, len);
    assert(s->data, "arena_alloc failed");
    s->cap = len;
}

template <typename T, typename A>
requires is_arena_allocator<A>
void slice_append(slice<T>* s, T v, A* a)
{
    if (s->len == s->cap) {
        static const size_t default_cap = 4;
        size_t new_cap = s->cap ? s->cap * 2 : default_cap;

        T* new_data = arena_alloc<T>(a, new_cap);
        if (s->data) {
            memcpy(new_data, s->data, sizeof(T) * s->len);
        }
        s->data = new_data;
        s->cap = new_cap;
    }
    
    s->data[s->len] = v;
    s->len += 1;

#ifndef NDEBUG
    std::cout << "[slice_append] {";
    for (size_t i = 0; i < s->len; i++) {
        std::cout << s->data[i];
        if (i + 1 != s->len) {
            std::cout << ", ";
        }
    }
    std::cout << "}" << std::endl;
#endif
}

template <typename T>
slice<T> slice_slice(slice<T> s, int beg, int end)
{
    assert(0 <= beg && beg <= end && end <= s.len, 
        "index out of bound! in:[%d, %d), valid:[%d, %d)", 
        beg, end, 0, s.len);
    return slice<T>{
        .data = s.data[beg],
        .len  = end - beg,
        .cap  = end - beg,
    };
}



#endif