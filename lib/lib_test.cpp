#include "lib.h"
#include "testing/testing.h"

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
    const size_t size = count * sizeof(int);
    arena a = arena_new(size, ARENA_STRATEGY_SILENT);
    defer { arena_free(&a); };
    
    int* alloc_result[count] ;
    for (size_t i = 0; i < count; i++) {
        auto p = arena_alloc<int>(&a);
        alloc_result[i] = p;
        *p = i;
        t->assert(p, "alloc failed at %d", i);
    }
    t->logf("alloc_result: [%d, %d, %d]", 
        *alloc_result[0], *alloc_result[1], *alloc_result[2]);
    
    auto p = arena_alloc<int>(&a);
    t->assert(!p, "alloc should fail silently after exahusted");
    
    arena_reset(&a);
    t->assert(arena_alloc<int>(&a), "alloc after reset should succeed");
}

int main(void)
{
    test_main({
        test_case{ "defer", nullptr, test_defer },
        test_case{ "alloc", nullptr, test_alloc },
    });
}