#ifndef ARENA_H
#define ARENA_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include "err.h"

using byte = uint8_t;

using arena_strategy = int;
#define ARENA_STRATEGY_PANIC 0
#define ARENA_STRATEGY_SILENT 1

#ifdef OLD_ARENA
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
#endif

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

basic_arena_chunk* make_chunk(int32_t size, basic_arena_chunk* next);

void chunk_release(basic_arena_chunk* chunk);

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

template<typename A>
concept is_arena_allocator = requires(A* a, int32_t n)
{
    arena_alloc<int>(a, n);
};

struct scratch_arena;

struct basic_arena {
    basic_arena_chunk* head;
    int32_t chunk_count;
    int32_t chunk_count_max;
    arena_strategy strat;
};

void basic_arena_make(
    basic_arena* a,
    int32_t init_size = 4 * KB, 
    int32_t max_chunks = 2,
    arena_strategy strategy = ARENA_STRATEGY_PANIC
);

void basic_arena_drop(basic_arena* a);
    
void* basic_arena_oom(const arena_strategy strat);

template<typename T>
T* arena_alloc(basic_arena* a, int32_t n)
{
    static const int32_t alignment = alignof(T);
    const int32_t data_size = sizeof(T) * n;
    assert(a->head, "arena not initialized");

    // fast path
    maybe<T*> mret = chunk_alloc_heap<T>(a->head, data_size, alignment);
    if (mret) {
        return mret.val;
    }
    
    // traverse other chunks 
    auto total_cap = a->head->cap;
    for (auto chunk = a->head->next; chunk; chunk = chunk->next) {
        maybe<T*> mret = chunk_alloc_heap<T>(chunk, data_size, alignment);
        if (mret) {
            return mret.val;
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
    return mret.val;
}
void basic_arena_reset_heap(basic_arena* a, bool shrink = false);

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

struct scratch_arena {
    basic_arena_chunk* chunk;
    int32_t old_stack_size;
    arena_strategy strat;
};

void scratch_arena_drop(scratch_arena* a);

template<typename T>
T* arena_alloc(scratch_arena* a, int32_t n)
{
    static const int32_t alignment = alignof(T);
    const int32_t data_size = sizeof(T) * n; 
    maybe<T*> mret = chunk_alloc_stack<T>(a->chunk, data_size, alignment);
    if (!mret) {
        return (T*)basic_arena_oom(a->strat);
    }
    return mret.val;
}

scratch_arena basic_arena_scratch(basic_arena* a);

#endif