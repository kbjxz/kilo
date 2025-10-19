#include <cmath>
#include <concepts>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <array>
#include <tuple>
#ifndef NDEBUG
#include <iostream>
#endif

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

inline void panic(void)
{
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
    
    operator bool()
    {
        return ok; 
    }
    T operator() ()
    {
        return value;
    }
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
    
    operator bool ()
    {
        return !merr;
    }
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

#define assert(cond, fmt, ...)\
internal::__assert(__FILE__, __LINE__, cond, fmt __VA_OPT__(,) __VA_ARGS__)
namespace internal {
inline void __assert(
    std::decay_t<decltype(__FILE__)> file,
    std::decay_t<decltype(__LINE__)> line,
    bool cond, const char* fmt, ...
)
{
#ifndef NDEBUG
    if (cond) {
        return;
    } 

    printf("panic: ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n\t%s:%d\n", file, line);
    
    panic();
#endif
}
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

#define KB 1024

/* 
    chunk anatomy (not to scale):
            heap_size                               stack_size
        /              \                        /               \      
        |      ...     |      free space        |      ...      |  basic_arena_chunk  |
        ^                                                       ^          |
        .data                                            .data+cap         |    
        |__________________________________________________________________|
*/ 
struct basic_arena_chunk {
    basic_arena_chunk* next;
    byte* data;
    int32_t heap_size;
    int32_t stack_size;
    int32_t cap;
};

static inline basic_arena_chunk* 
make_chunk(int32_t size, basic_arena_chunk* next)
{
    auto block_size = size + sizeof(basic_arena_chunk);
    auto block = malloc(block_size);
    assert(block, "malloc");
    auto chunk = (basic_arena_chunk*)((byte*)(block)+size);
    *chunk = basic_arena_chunk{
        .next = next, 
        .data = (byte*)block,
        .heap_size = 0,
        .stack_size = 0,
        .cap = size,
    };
    return chunk;
}

static inline void chunk_release(basic_arena_chunk* chunk) 
{
    if (chunk->data) {
        free(chunk->data);
    }
}

/*  chunk_alloc_heap anatomy:
    ----------------------------------------------------------------------------
    |     heap data    |   padding  |  new data  | unused |     stack data     |
    ----------------------------------------------------------------------------
    ^                  ^            ^                     ^                    ^
    |                  |            |                     |                    |
    data       data+heap_size      ret              data+cap-stack_size        data+cap
*/
template<typename T>
maybe<T*> chunk_alloc_heap(basic_arena_chunk* chunk, int32_t data_size, int32_t alignment)
{
    byte* curr = chunk->data + chunk->heap_size;
    const int32_t extra = int64_t(curr) % alignment;
    const int32_t padding = (extra == 0 ? 0 : alignment - extra);
    const int32_t alloc_size = data_size + padding;
    
    if ((chunk->heap_size + alloc_size + chunk->stack_size) > chunk->cap) {
        return none<T*>();
    }
    
    byte* ret = curr + padding;
    assert(int64_t(ret) % alignment==0,
        "[align] ret:%d, alignof:%d", ret, alignment);
    memset(ret, 0, data_size);
    chunk->heap_size += alloc_size;
    return some((T*)(void*)ret);
}

template<typename T, typename A>
T* arena_alloc(A* arena, int32_t n);

template<typename T, typename A>
concept is_arena_allocator = requires(A* a, int32_t n)
{
    arena_alloc<T>(a, n);
};

struct scratch_arena;

struct basic_arena {
    basic_arena_chunk* head;
    int32_t chunk_count;
    int32_t chunk_count_max;
    arena_strategy strat;
};

inline void basic_arena_make(
    basic_arena* a,
    int32_t init_size = 4 * KB, 
    int32_t max_chunks = 2,
    arena_strategy strategy = ARENA_STRATEGY_PANIC
)
{
    assert(max_chunks > 0, "non-positive max_chunks");
    a->head = make_chunk(init_size, NULL);
    a->chunk_count = 1;
    a->chunk_count_max = max_chunks;
    a->strat = strategy; 
};

inline void basic_arena_drop(basic_arena* a)
{
    basic_arena_chunk* prev = NULL;
    basic_arena_chunk* curr = a->head;
    while (curr) {
        prev = curr;
        curr = curr->next;
        chunk_release(prev);
    }
}
    
static void* basic_arena_oom(const arena_strategy strat)
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

template<typename T, typename A = basic_arena>
T* arena_alloc(basic_arena* a, int32_t n)
{
    static const int32_t alignment = alignof(T);
    const int32_t data_size = sizeof(T) * n;
    assert(a->head, "arena not initialized");

    // fast path
    maybe<T*> mret = chunk_alloc_heap<T>(a->head, data_size, alignment);
    if (mret) {
        return mret();
    }
    
    // traverse other chunks 
    auto total_cap = a->head->cap;
    for (auto chunk = a->head->next; chunk; chunk = chunk->next) {
        maybe<T*> mret = chunk_alloc_heap<T>(chunk, data_size, alignment);
        if (mret) {
            return mret();
        }
        total_cap += chunk->cap;
    }
    
    // check max chunk before try allocating new chunk
    if (a->chunk_count == a->chunk_count_max) {
        return (T*)basic_arena_oom(a->strat); 
    }
    
    // allocate a new chunk and push it front
    const int32_t min_size = sizeof(T) * n;
    const int32_t new_chunk_size = min_size < total_cap  ? total_cap : min_size * 2;
    a->head = make_chunk(new_chunk_size, a->head);
    a->chunk_count++;
    mret = chunk_alloc_heap<T>(a->head, data_size, alignment);
    assert(bool(mret), "alloc failed! size: %d=(%d:n)*(%d:sizeof(T)), chunk->cap: %d",
        min_size, n, sizeof(T), a->head->cap);
    return mret();
}

inline void basic_arena_reset_heap(basic_arena* a, bool shrink = false)
{
    basic_arena_chunk* tmp = NULL;
    basic_arena_chunk* curr = a->head;
    while (curr) {
        curr->heap_size = 0;
        tmp = curr;
        curr = curr->next;
        
        if (!shrink) {
            continue;
        }
        
        if (tmp == a->head) {
            tmp->next = NULL; 
        } else {
            chunk_release(tmp);
        }
    }
};

/*  chunk_alloc_stack anatomy:
    -----------------------------------------------------------------------------
    |     heap data    |   unused  | new data | padding |        stack data     |
    -----------------------------------------------------------------------------
    ^                  ^           ^                    ^                       ^
    |                  |           |                    |                       |
    data   data+heap_size          ret            data+cap-stack_size       data+cap
*/
template<typename T>
maybe<T*> chunk_alloc_stack(basic_arena_chunk* chunk, int32_t data_size, int32_t alignment)
{
    const int32_t padding = int64_t(chunk->data + chunk->stack_size) % alignment;
    const int32_t alloc_size = data_size + padding;
    if ((chunk->heap_size + chunk->stack_size + alloc_size) > chunk->cap) {
        return none<T*>();
    }
    
    byte* ret = (chunk->data + chunk->cap) - (chunk->stack_size + alloc_size);
    assert(int64_t(ret) % alignment==0,
        "[align] ret:%d, alignof:%d", ret, alignment);
    memset(ret, 0, data_size);
    chunk->stack_size += alloc_size;
    return some((T*)(void*)ret);
}

template<typename T>
T* scratch_arena_alloc(scratch_arena* a, int32_t n);

struct scratch_arena {
    basic_arena_chunk* chunk;
    int32_t old_stack_size;
    arena_strategy strat;
};

inline void scratch_arena_drop(scratch_arena* a)
{
    a->chunk->stack_size = a->old_stack_size;
}

template<typename T, typename A = scratch_arena>
T* arena_alloc(scratch_arena* a, int32_t n)
{
    static const int32_t alignment = alignof(T);
    const int32_t data_size = sizeof(T) * n; 
    maybe<T*> mret = chunk_alloc_stack<T>(a->chunk, data_size, alignment);
    if (!mret) {
        return (T*)basic_arena_oom(a->strat);
    }
    return mret();
}

inline scratch_arena basic_arena_scratch(basic_arena* a)
{
    scratch_arena s = {};
    s.chunk = a->head;
    s.old_stack_size = a->head->stack_size;
    s.strat = a->strat;
    return s;
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

template <typename T, typename A>
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
requires is_arena_allocator<T, A>
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


template <typename K, typename V, typename A>
struct hashmap {
    using equal_func = bool(*)(const K*, const K*);
    using hash_func = size_t(*)(const K);
    
    static constexpr int load_factor = 6; // => 0.6
    
    int32_t len;
    int32_t cap;
    hash_func hash;
    equal_func equal;
    A* arena;
    slice<maybe<K>> keys;
    slice<V> vals;
    
    maybe<V*> operator[](const K* key);
};

template <typename K, typename V, typename _>
struct hashmap_iter {
    int32_t pos;
    hashmap<K, V, _>* hm;
    
    inline void operator++()
    {
        pos++;
        for (; pos != hm->cap && !hm->keys[pos]; pos++) {
        }
    }
    
    using hashmap_kv = std::tuple<const K*, V*>;
    inline hashmap_kv operator*() 
    {
        return std::make_tuple(&hm->keys[pos].value, &hm->vals[pos].value);
    }
};

template <typename K, typename V, typename A>
void make_hashmap(
    hashmap<K, V, A>* hm,
    A* arena,
    typename hashmap<K, V, A>::hash_func hash,
    typename hashmap<K, V, A>::equal_func equal,
    int32_t cap = 32
)
{
    hm->len = 0;
    hm->cap = cap;
    hm->hash = hash;
    hm->equal = equal;
    hm->arena = arena;

    hm->keys = {};
    slice_reserve(&hm->keys, cap, arena);

    hm->vals = {};
    slice_reserve(&hm->vals, cap, arena);
}

// linear probing
template <typename K, typename V, typename _>
bool __hashmap_put_noresize(
    slice<maybe<K>>* keys, slice<V>* vals,
    const K* key, const V* val,
    typename hashmap<K, V, _>::hash_func hash,
    typename hashmap<K, V, _>::equal_func equal
)
{
    const int32_t cap = keys->cap;
    int32_t pos = hash(key) % cap;
    for (auto i = pos; i != pos + cap; i++) {
        auto mkey = keys[i%cap];
        if (!mkey) {
            keys[i] = some(*key);
            vals[i] = *val;
            return true;
        } else if (equal(key, &mkey->value)) {
            vals[i] = *val;
            return true;
        }
    }
    return false;
}


template <typename K, typename V, typename A>
void __hashmap_resize(hashmap<K, V, A>* hm)
{
    const int32_t new_cap = hm->cap * 2 + 1;
    slice<maybe<K>> new_keys = {};
    slice_reserve(&new_keys, new_cap, hm->arena);
    slice<V> new_vals = {};
    slice_reserve(&new_vals, new_cap, hm->arena);
    
    for (int32_t i = 0; i < hm->cap; i++) {
        if (!hm->keys[i]) {
            continue;
        }
        
        auto ok = __hashmap_put_noresize(
            &new_keys, &new_vals, 
            &hm->keys[i], &hm->vals[i], 
            hm->hash, hm->equal);
        assert(ok, "move kv failed");
    }
    
    hm->cap = new_cap;
    hm->keys = new_keys;
    hm->vals = new_vals;
}

template <typename K, typename V, typename A>
void hashmap_put(hashmap<K, V, A>* hm, const K* key, const V* val)
{
    if (hm->len * 10 / hm->cap > hm->load_factor) {
        __hashmap_resize(hm);
    }

    auto ok =__hashmap_put_noresize(
        &hm->keys, &hm->vals, key, val, 
        hm->hash, hm->equal);
    assert(ok, "put failed");
}

template <typename K, typename V, typename _>
maybe<V*> hashmap_get(const hashmap<K, V, _>* hm, const K* key)
{
    const int32_t beg = hm->hash(key) % hm->cap;
    const int32_t end = beg + hm->cap;
    for (auto i = beg; i != end; i++) {
        auto mkey = hm->keys[i];
        if (mkey && equal(key, &mkey->value)) {
            return some(hm->vals[i]);
        }
    }
    return none<V*>();
}

template <typename K, typename V, typename _>
maybe<V*> hashmap<K, V, _>::operator[](const K* key)
{
    return hashmap_get(this, key);
}
    
template <typename K, typename V, typename _>
hashmap_iter<K, V, _> hashmap_beg(hashmap<K, V, _>* hm)
{
    if (hm->len == 0) {
        return hashmap_end(hm);
    }
    int32_t pos = 0;
    for (; !hm->keys[pos]; pos++) {}
    return hashmap_iter{.pos = pos, .hm = hm};
}

template <typename K, typename V, typename _>
hashmap_iter<K, V, _> hashmap_end(hashmap<K, V, _>* hm)
{
    return hashmap_iter{.pos = hm->cap, .hm = hm};
}

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_key(const void* key, size_t n) {
    uint64_t hash = FNV_OFFSET;
    const auto ckey = (unsigned char*)key;
    for (size_t i = 0; i < n; i++) {
        hash ^= (uint64_t)(ckey[i]);
        hash *= FNV_PRIME;
    }
    return hash;
}

template <typename T>
uint64_t hash_key(const T* key)
{
    return hash_key(key, sizeof(T));
}

template <typename T>
uint64_t hash_key(const slice<T>* key)
{
    return hash_key(key->data, key->len);
}
