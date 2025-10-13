#include <concepts>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <array>

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

template <typename T>
struct maybe {
    T value;
    bool ok;
};

template <typename T>
maybe<T> some(T v)
{
    return maybe<T>{.value = v, .ok = true};
}

template <typename T>
maybe<T> none()
{
    return maybe<T>{.value = {}, .ok = false};
}

template <typename T, typename Err = error>
struct result {
    maybe<Err> merr;
    T value;
};

template <typename T, typename Err = error>
result<T, Err> result_err(Err e)
{
    return result<T, Err>{.merr = some(e), .value ={}};
}

template <typename T, typename Err = error>
result<T, Err> result_v(T v)
{
    return result<T, Err>{.merr = none<Err>(), .value =v};
}

template <typename _, typename Err>
bool result_is_err(const result<_, Err>* r)
{
    return r->merr.ok;
}

template <typename _, typename Err>
Err result_unwrap_err(const result<_, Err>* r)
{
    return r->merr.value;
}


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

inline void arena_scratch_from(arena* scratch, const arena* from)
{
    *scratch = *from;
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

    T& operator[](size_t i) 
    {
        assert(i < this->len, "out of bound! len:%d, i:%d", this->len, i);
        return this->data[i];
    }
};

template <typename T>
void slice_reserve(slice<T>* s, size_t new_cap, arena* a)
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

template <typename T>
void slice_append(slice<T>* s, T v, arena* a)
{
    if (s->len < s->cap) {
        s->data[s->len] = v;
        s->len++;
    }
    
    static const size_t default_cap = 4;
    size_t new_cap = s->cap ? s->cap * 2 : default_cap;

    T* new_data = arena_alloc<T>(a, new_cap);
    if (s->data) {
        memcpy(new_data, s->data, s->len);
    }

    s->data = new_data;
    s->len += 1;
    s->cap = new_cap;
}

template <typename T>
slice<T> slice_sub(slice<T> s, int beg, int end)
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

using string = slice<char>;

template <std::size_t N>
const string string_from(const std::array<char, N>& a)
{
    return string{
        .data = (char*)((void*)(a.data())),
        .len  = a.size() - 1,
        .cap  = a.size() - 1,
    };
}

template <std::size_t N>
const string string_from(const char (&a)[N])
{
    return string_from(std::to_array(a));
}

inline void string_reserve(string* s, size_t new_cap, arena* a)
{
    slice_reserve(s, new_cap, a);
}

inline void string_append(string* s, const char* v, size_t n, arena* a)
{
    slice_reserve(s, s->len+ n, a);
    memcpy(&s->data[s->len], v, n);
}

inline void string_append(string* s, char c, arena* a)
{
    slice_append(s, c, a);
}

inline void string_append(string* s, const string* oth, arena* a)
{
    slice_reserve(s, s->len + oth->len, a);
    memcpy(&s->data[s->len], oth->data, oth->len);  
}