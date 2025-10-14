#include <concepts>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <array>
#include <utility>

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
    slice_reserve(s, s->len + n, a);
    memcpy(&s->data[s->len], v, n);
    s->len += n;
}

inline void string_append(string* s, char c, arena* a)
{
    slice_append(s, c, a);
}

inline void string_append(string* s, const string* oth, arena* a)
{
    slice_reserve(s, s->len + oth->len, a);
    memcpy(&s->data[s->len], oth->data, oth->len);  
    s->len += oth->len;
}

#define KB 1024

struct basic_arena_chunk {
    basic_arena_chunk* next;
    byte* data;
    int32_t len;
    int32_t cap;
};

static inline basic_arena_chunk* 
make_chunk(
    int32_t size, 
    basic_arena_chunk* next
)
{
    auto block_size = size + sizeof(basic_arena_chunk);
    auto block = malloc(block_size);
    assert(block, "malloc");
    auto chunk = (basic_arena_chunk*)((byte*)(block)+size);
    chunk->next = next; 
    chunk->data = (byte*)block;
    chunk->len = 0;
    chunk->cap = size;
    return chunk;
}

static inline void release_chunk(basic_arena_chunk* chunk) 
{
    if (chunk->data) {
        free(chunk->data);
    }
}

template<typename T>
maybe<T*> chunk_alloc_heap(basic_arena_chunk* chunk, int32_t n = 1)
{
    byte* curr = chunk->data + chunk->len;
    const ptrdiff_t alignment = alignof(T);
    const ptrdiff_t extra = ptrdiff_t(curr) % alignment;
    const ptrdiff_t padding = 
        extra == 0 ? 0 : alignment - extra;

    
    const ptrdiff_t size = sizeof(T) * n;
    
    const auto new_len = chunk->len + size;
    if (new_len > chunk->cap) {
        return none<T*>();
    }
    
    byte* ret = curr + padding;
    assert(ptrdiff_t(ret)%alignment==0,
        "[align] ret:%d, alignof:%d", ret, alignment);
    memset(ret, 0, size);
    chunk->len += size;
    return some<T*>(ret);
}

struct scratch_arena;

struct basic_arena {
    basic_arena_chunk* head;
    int32_t chunk_count;
    const int32_t chunk_count_max;
    const arena_strategy strat;
    
    basic_arena
    (
        int32_t init_size = 4 * KB, 
        int32_t max_chunks = 4,
        arena_strategy strategy = ARENA_STRATEGY_PANIC
    )
        : head(make_chunk(init_size, NULL))
        , chunk_count(1)
        , chunk_count_max(max_chunks)
        , strat(strategy) 
    {
        assert(max_chunks > 0, "non-positive max_chunks");
    };

    ~basic_arena()
    {
        basic_arena_chunk* prev = NULL;
        basic_arena_chunk* curr = head;
        while (curr) {
            prev = curr;
            curr = curr->next;
            release_chunk(prev);
        }
    }
    
    static void* oom(arena_strategy strat)
    {
        switch (strat) {
        case ARENA_STRATEGY_PANIC:
            panic("arena chunks exhausted");
        break; case ARENA_STRATEGY_SILENT:
            return NULL;
        default:
            panic("unknown strategy");
        }
        return NULL;
    }
    
    template<typename T>
    T* alloc(int n)
    {
        assert(head, "arena not initialized");

        // fast path
        maybe<T*> mret = chunk_alloc_heap<T>(head, n);
        if (mret.ok) {
            return mret.value;
        }
        
        // traverse other chunks 
        auto total_cap = head->cap;
        for (auto chunk = head->next; chunk; chunk = chunk->next) {
            maybe<T*> mret = chunk_alloc_heap<T>(chunk, n);
            if (mret.ok) {
                return mret.value;
            }
            total_cap += chunk->cap;
        }
        
        // check max chunk before try allocating new chunk
        if (chunk_count == chunk_count_max) {
            return oom(strat); 
        }
        
        // allocate a new chunk and push it front
        const auto min_size = sizeof(T) * n;
        const auto new_chunk_size = min_size < total_cap  ? total_cap : min_size * 2;
        head = make_chunk(new_chunk_size, head);
        mret = chunk_alloc_heap<T>(head, n);
        assert(mret.ok, "alloc failed! size: %d=(%d:n)*(%d:sizeof(T)), chunk->cap: %d",
            min_size, n, sizeof(T), head->cap);
        return mret.value;
    }

    void reset(bool shrink = false)
    {
        basic_arena_chunk* tmp = NULL;
        basic_arena_chunk* curr = head;
        while (curr) {
            curr->len = 0;
            tmp = curr;
            curr = curr->next;
            
            if (!shrink) {
                continue;
            }
            
            if (tmp == head) {
               tmp->next = NULL; 
            } else {
                release_chunk(tmp);
            }
        }
    };
    
    scratch_arena get_scratch(void);
};

template<typename T>
maybe<T*> chunk_alloc_stack(basic_arena_chunk* chunk, int32_t n = 1)
{
    byte* curr = chunk->data - chunk->len;
    const ptrdiff_t alignment = alignof(T);
    const ptrdiff_t padding = ptrdiff_t(curr) % alignment;
    const ptrdiff_t size = sizeof(T) * n;
    const auto new_len = chunk->len + size;
    if (new_len > chunk->cap) {
        return none<T*>();
    }
    
    byte* ret = curr - padding;
    assert(ptrdiff_t(ret)%alignment==0,
        "[align] ret:%d, alignof:%d", ret, alignment);
    memset(ret, 0, size);
    chunk->len += size;
    return some<T*>(ret);
}

struct scratch_arena {
    basic_arena_chunk stack;
    const arena_strategy strat;
    
    template<typename T>
    T* alloc(int n)
    {
        maybe<T*> mret = chunk_alloc_stack<T>(&stack, n);
        if (!mret.ok) {
            return basic_arena::oom(strat);
        }
        return mret.value;
    }
};

inline scratch_arena basic_arena::get_scratch(void)
{
    return scratch_arena{
        .stack = {
            .next = NULL,
            .data = head->data + head->cap,
            .len = 0,
            .cap = head->cap - head->len,
        },
        .strat = strat,
    };
}