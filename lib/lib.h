#include <concepts>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

template<typename T>
requires std::is_invocable_v<T>
struct __defer: T {
    [[gnu::always_inline]]
    __defer(T g) : T(g) 
    {}

    [[gnu::always_inline]]
    ~__defer() 
    {
        T::operator()();
    }
};
 
#define __DEFER__(V)  __defer const V = [&](void)->void


#define defer __DEFER(__COUNTER__)
#define __DEFER(N) __DEFER_(N)
#define __DEFER_(N) __DEFER__(__DEFER_VARIABLE_ ## N)

inline void panic(const char* s)
{
    perror(s);
    exit(1);
}

template <typename F>
requires std::is_invocable_v<F>
void panic(const char* s, F cleanup)
{
    cleanup();
    perror(s);
    exit(1);
}

struct error {
    int err_no;
    const char* msg;
};

template <typename T, typename Err = error>
struct result {
    Err error;
    T value;
};

inline void assert(bool cond, const char* fmt, ...)
{
#ifndef NDEBUG
    if (cond) {
        return;
    } 

    printf("\t%s:%d: s", __FILE__, __LINE__);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
#endif
}

#include <stddef.h>
#include <string.h>

using byte = uint8_t;

using arena_strategy = int;
#define ARENA_STRATEGY_PANIC 0
#define ARENA_STRATEGY_SILENT 1

struct arena {
    byte* data;
    byte* curr;
    byte* end;
    arena_strategy strat;
};

inline arena arena_new(
    size_t size = 4096, 
    arena_strategy strategy = ARENA_STRATEGY_PANIC)
{
    byte* data = static_cast<byte*>(malloc(size));
    assert(data, "malloc");
    return {
        .data = data,       
        .curr = data,
        .end = data+size,
        .strat = strategy,
    };
}

inline void arena_free(arena* a)
{
    if (a->data) {
        free(static_cast<void*>(a->data));
        *a = {};
    }
}

inline void arena_reset(arena* a)
{
    if (a->data) {
        a->curr = a->data;
    }
}

template <typename T>
T* arena_alloc(arena* a, int n = 1)
{
    const ptrdiff_t alignment = alignof(T);
    const ptrdiff_t extra = ptrdiff_t(a->curr)%alignment;
    const ptrdiff_t padding = 
        extra == 0 ? 0 : alignment - extra;

    byte* ret = (a->curr) + (padding);
    assert(ptrdiff_t(ret)%alignment==0,
         "[align] ret:%d, alignof:%d", ret, alignment);
    const ptrdiff_t size = sizeof(T) * n;
    
    byte* next = ret + size;
    if (next > a->end) {
        switch (a->strat) {
        case ARENA_STRATEGY_PANIC:
            assert(false, "oom");
        break; case ARENA_STRATEGY_SILENT:
            return 0;
        }
    }
    
    memset(ret, 0, size);

    a->curr = next;
    
    return (T*)(ret);
}

template <typename T>
struct slice {
    T* data;
    size_t len;
    size_t cap;

    void append(T v, arena* a)
    {
        if (this->len < this->cap) {
            this->data[this->len] = v;
            this->len++;
        }
        
        static const size_t default_cap = 4;
        size_t new_cap = this->cap ? this->cap * 2 : default_cap;

        byte* new_data = arena_alloc<T>(a, new_cap);
        if (this->data) {
            memcpy(new_data, this->data, this->len);
        }

        this->data = new_data;
        this->len += 1;
        this->cap = new_cap;
    }
    
    T& operator[](int i) 
    {
        assert(i < this->len, "out of bound! len:%d, i:%d", this->len, i);
        return this->data[i];
    }
};


