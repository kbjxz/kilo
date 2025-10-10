#include "lib.h"
#include "testing.h"
#include <array>

void test_defer(test_handler* t)
{
    auto counter = 0;
    {
        counter++;
        defer
        {
            counter--;
        };
    }
    t->assert(counter == 0, "[counter] exp: 0, got: %d", counter);
}

void test_alloc(test_handler* t)
{
    constexpr size_t count = 3;
    constexpr size_t size = count * sizeof(int);
    arena a = arena_new(size, ARENA_STRATEGY_SILENT);
    defer { arena_free(&a); };
    
    int* last_ptr = 0;
    for (size_t i = 0; i < count; i++) {
        auto p = arena_alloc<int>(&a);
        t->assert(p, "alloc failed at %d", i);
        
        if (last_ptr && p) {
            auto step = ptrdiff_t(p) - ptrdiff_t(last_ptr);
            t->assert(sizeof(int) == step, 
            "[step] exp: %d, got: %d; last: %d, this: %d", sizeof(int), step, last_ptr, p);
        } 

        last_ptr = p;
    }
    
    auto p = arena_alloc<int>(&a);
    t->assert(!p, "alloc should fail silently after exahusted");
    
    arena_reset(&a);
    t->assert(arena_alloc<int>(&a), "alloc after reset should succeed");
}

void test_slice_append(test_handler* t)
{
    constexpr int count = 10;
    constexpr size_t size  = sizeof(int) * count;
    constexpr std::array<int, count> exp = {
        0,1,2,3,4,5,6,7,8,9};
    arena a = arena_new(size);
    slice<int> s = {};

    for (auto i = 0; i < count; i++) {
        slice_append(&s, i, &a);
    }
    
    auto is_len_equal = s.len == count;
    t->assert(is_len_equal, "[len] exp: %d, got: %d", count, s.len);
    if (!is_len_equal) {
        return;
    }
    for (auto i = 0; i < count; i++) {
        t->assert(s[i] == exp[i], "[%d] exp: %d, got: %d", s[i], exp[i]);
    }
}

int main(void)
{
    test_main({
        new_case( "defer", test_defer ),
        new_case( "alloc", test_alloc ),
        new_case( "test_slice_append", test_alloc ),
    });
}
