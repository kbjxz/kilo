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
        t->assert(*s[i] == exp[i], "[%d] exp: %d, got: %d", s[i], exp[i]);
    }
}

template <typename T, size_t size, size_t alignment>
constexpr void static_assert_sa(void)
{
    static_assert(sizeof(T) == size);
    static_assert(alignof(T) == alignment);
}

void test_basic_arena(test_handler* t)
{
    t->subtest("alloc array and exhausts one block", [](test_handler* t) {
        const int32_t chunk_size = 4096;
        basic_arena a = basic_arena(chunk_size, 2, ARENA_STRATEGY_SILENT);
        
        const int32_t array_len = chunk_size / sizeof(int32_t);
        int32_t* array = a.alloc<int32_t>(array_len); 
        for (auto i = 0; i < array_len; i++) {
            t->assert(array[i] == 0, "array[%d]=%d", i, array[i]);
        }
    });
    
    t->subtest("alloc array and exhausts all possible chunks", [](test_handler* t) {
        const int32_t chunk_size = 128;
        const int32_t max_chunks = 2;
        const int32_t max_allocs = 3; // chunks->[size:128]->[size:256]
        struct s { int32_t v[chunk_size / sizeof(int32_t)]; };

        basic_arena a = basic_arena(chunk_size, max_chunks, ARENA_STRATEGY_SILENT);
        
        for (auto i = 0; i < max_allocs; i++) {
            auto p = a.alloc<s>(1); 
            t->assert(p, "alloc[%d] failed", i);
        }

        t->assert(!a.alloc<s>(1), "should failed on last alloc");
    });
    
    t->subtest("alloc heap and stack", [](test_handler* t){
        const int32_t chunk_size = 128;
        const int32_t max_heap_size = chunk_size / 2;
        const int32_t max_stack_size = chunk_size - max_heap_size;
        
        basic_arena a = basic_arena(chunk_size, 1 , ARENA_STRATEGY_SILENT);
        t->assert(a.alloc<char>(max_heap_size), "heap alloc failed");
        
        {
            auto scratch = a.scratch();
            auto stack_data = scratch.alloc<char>(max_stack_size);
            t->assert(stack_data, "stack alloc failed");
            
            t->assert(!a.alloc<char>(1), "heap alloc succeeded unexpectedly");
            t->assert(!scratch.alloc<char>(1), "stack alloc succeeded unexpectedly");
        }
        
        t->assert(a.alloc<char>(1), "heap alloc failed after scratch released");
    });

    t->subtest("mixed sizes", [](test_handler* t) {
        const int32_t chunk_size = 1024;
        basic_arena a = basic_arena(chunk_size);
        
        struct s1a1 { int8_t v; }; 
        static_assert_sa<s1a1, 1, 1>();
        auto p1 = a.alloc<s1a1>(1);
        t->assert(p1, "s1a1");
        t->logf("&s1a1: %p", p1);
        

        struct s7a1 {
            int8_t v[7];
        };
        static_assert_sa<s7a1, 7, 1>();
        auto p2 = a.alloc<s7a1>(1);
        t->assert(p2, "s7a1");
        t->logf("&s7a1: %p", p2);

        struct s12a4 {
            int32_t v1[2];
            int8_t v3;
        };
        static_assert_sa<s12a4, 12, 4>();
        auto p3 = a.alloc<s12a4>(1);
        t->assert(p3, "s12a4");
        t->logf("&s12a4: %p", p3);

        struct s24a8 {
            int64_t v[3];
        };
        static_assert_sa<s24a8, 24, 8>();
        auto p4 = a.alloc<s24a8>(1);
        t->assert(p4, "s24a8");
        t->logf("&s24a8: %p", p4);
    });
}

int main(void)
{
    test_main({
        new_case("defer", test_defer),
        new_case("alloc", test_alloc),
        new_case("test_slice_append", test_alloc),
        new_case("test_basic_arena", test_basic_arena),
    });
}
