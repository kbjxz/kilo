#include "arena.h"

basic_arena_chunk* 
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

void chunk_release(basic_arena_chunk* chunk) 
{
    if (chunk->data) {
        free(chunk->data);
    }
}

void basic_arena_make(
    basic_arena* a,
    int32_t init_size, 
    int32_t max_chunks,
    arena_strategy strategy
)
{
    assert(max_chunks > 0, "non-positive max_chunks");
    a->head = make_chunk(init_size, NULL);
    a->chunk_count = 1;
    a->chunk_count_max = max_chunks;
    a->strat = strategy; 
}

void basic_arena_drop(basic_arena* a)
{
    basic_arena_chunk* prev = NULL;
    basic_arena_chunk* curr = a->head;
    while (curr) {
        prev = curr;
        curr = curr->next;
        chunk_release(prev);
    }
}

void* basic_arena_oom(const arena_strategy strat)
{
    switch (strat) {
    case ARENA_STRATEGY_PANIC:
        panic_errno("arena chunks exhausted");
    break; case ARENA_STRATEGY_SILENT:
        return NULL;
    default:
        panic_errno("unknown strategy");
    }
    return NULL;
}

void basic_arena_reset_heap(basic_arena* a, bool shrink)
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


void scratch_arena_drop(scratch_arena* a)
{
    a->chunk->stack_size = a->old_stack_size;
}

scratch_arena basic_arena_scratch(basic_arena* a)
{
    scratch_arena s = {};
    s.chunk = a->head;
    s.old_stack_size = a->head->stack_size;
    s.strat = a->strat;
    return s;
}