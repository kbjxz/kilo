#include "lib.h"
#include <array>

void test_defer(testing::handle* t)
{
    auto counter = 0;
    {
        counter++;
        defer
        {
            counter--;
        };
    }
    testing::assertf(t, counter == 0, "[counter] exp: 0, got: %d", counter);
}

#ifdef OLD_ARENA
void test_alloc(testing::handle* t)
{
    constexpr size_t count = 3;
    constexpr size_t size = count * sizeof(int);
    arena a = arena_new(size, ARENA_STRATEGY_SILENT);
    defer { arena_free(&a); };
    
    int* last_ptr = 0;
    for (size_t i = 0; i < count; i++) {
        auto p = arena_alloc<int>(&a);
        testing::assertf(t, p, "alloc failed at %d", i);
        
        if (last_ptr && p) {
            auto step = ptrdiff_t(p) - ptrdiff_t(last_ptr);
            testing::assertf(t, sizeof(int) == step, 
            "[step] exp: %d, got: %d; last: %d, this: %d", sizeof(int), step, last_ptr, p);
        } 

        last_ptr = p;
    }
    
    auto p = arena_alloc<int>(&a);
    testing::assertf(t, !p, "alloc should fail silently after exahusted");
    
    arena_reset(&a);
    testing::assertf(t, arena_alloc<int>(&a), "alloc after reset should succeed");
}
#endif

void test_slice_append(testing::handle* t)
{
    constexpr int count = 10;
    constexpr std::array<int, count> exp = {0,1,2,3,4,5,6,7,8,9};
    basic_arena a = {};
    basic_arena_make(&a, 1024);
    slice<int> s = {};

    // slice cap: 4, 8, 16
    for (auto i = 0; i < count; i++) {
        slice_append(&s, i, &a);
    }
    
    auto is_len_equal = s.len == count;
    testing::assertf(t, is_len_equal, "[len] exp: %d, got: %d", count, s.len);
    if (!is_len_equal) {
        return;
    }
    for (auto i = 0; i < count; i++) {
        testing::assertf(t, s[i] == exp[i], 
            "[%d] exp: %d, got: %d", 
            i, exp[i], s[i] 
        );
    }
}

template <typename T, size_t size, size_t alignment>
constexpr void assert_size_alignment(void)
{
    static_assert(sizeof(T) == size);
    static_assert(alignof(T) == alignment);
}

void test_basic_arena(testing::handle* t)
{
    subtest(t, "alloc array and exhausts one block", [](testing::handle* t) {
        const int32_t chunk_size = 4096;
        basic_arena a = {};
        basic_arena_make(&a, chunk_size, 2, ARENA_STRATEGY_SILENT);
        defer { basic_arena_drop(&a); };
        
        const int32_t array_len = chunk_size / sizeof(int32_t);
        int32_t* array = arena_alloc<int32_t>(&a, array_len); 
        for (auto i = 0; i < array_len; i++) {
            testing::assertf(t, array[i] == 0, "array[%d]=%d", i, array[i]);
        }
    });
    
    subtest(t, "reset", [](testing::handle* t) {
        const int32_t chunk_size = 1024;
        basic_arena a = {};
        basic_arena_make(&a, chunk_size, 1, ARENA_STRATEGY_SILENT);
        defer { basic_arena_drop(&a); };
        
        const int32_t n = chunk_size / sizeof(char);
        testing::assertf(t, arena_alloc<char>(&a, n), "heap alloc failed");
        testing::assertf(t, !arena_alloc<char>(&a, 1), "heap alloc should not succeed");

        basic_arena_reset_heap(&a);
        testing::assertf(t, arena_alloc<char>(&a, n), "heap alloc failed after reset");
    });
    
    subtest(t, "alloc array and exhausts all possible chunks", [](testing::handle* t) {
        const int32_t chunk_size = 128;
        const int32_t max_chunks = 2;
        const int32_t max_allocs = 3; // chunks->[size:128]->[size:256]
        struct s { int32_t v[chunk_size / sizeof(int32_t)]; };

        basic_arena a = {};
        basic_arena_make(&a, chunk_size, max_chunks, ARENA_STRATEGY_SILENT);
        defer { basic_arena_drop(&a); };
        
        
        for (auto i = 0; i < max_allocs; i++) {
            auto p = arena_alloc<s>(&a, 1); 
            testing::assertf(t, p, "alloc[%d] failed", i);
        }

        testing::assertf(t, !arena_alloc<s>(&a, 1), "should failed on last alloc");
    });
    
    subtest(t, "alloc heap and stack", [](testing::handle* t){
        const int32_t chunk_size = 128;
        const int32_t max_heap_size = chunk_size / 2;
        const int32_t max_stack_size = chunk_size - max_heap_size;
        
        basic_arena a = {};
        basic_arena_make(&a, chunk_size, 1 , ARENA_STRATEGY_SILENT);
        defer { basic_arena_drop(&a); };

        testing::assertf(t, arena_alloc<char>(&a, max_heap_size), "heap alloc failed");
        
        {
            auto scratch = basic_arena_scratch(&a);
            defer { scratch_arena_drop(&scratch); };
            auto stack_data = arena_alloc<char>(&scratch, max_stack_size);
            testing::assertf(t, stack_data, "stack alloc failed");
            
            testing::assertf(t, !arena_alloc<char>(&a, 1), "heap alloc succeeded unexpectedly");
            testing::assertf(t, !arena_alloc<char>(&scratch, 1), "stack alloc succeeded unexpectedly");
        }
        
        testing::assertf(t, arena_alloc<char>(&a, 1), "heap alloc failed after scratch released");
    });

    subtest(t, "mixed sizes", [](testing::handle* t) {
        const int32_t chunk_size = 1024;
        basic_arena a = {};
        basic_arena_make(&a, chunk_size);
        defer { basic_arena_drop(&a); };

        
        struct s1a1 { int8_t v; }; 
        assert_size_alignment<s1a1, 1, 1>();
        auto p1 = arena_alloc<s1a1>(&a, 1);
        testing::assertf(t, p1, "s1a1");
        testing::logf(t, "&s1a1: %p", p1);
        

        struct s7a1 {
            int8_t v[7];
        };
        assert_size_alignment<s7a1, 7, 1>();
        auto p2 = arena_alloc<s7a1>(&a, 1);
        testing::assertf(t, p2, "s7a1");
        testing::logf(t, "&s7a1: %p", p2);

        struct s12a4 {
            int32_t v1[2];
            int8_t v3;
        };
        assert_size_alignment<s12a4, 12, 4>();
        auto p3 = arena_alloc<s12a4>(&a, 1);
        testing::assertf(t, p3, "s12a4");
        testing::logf(t, "&s12a4: %p", p3);

        struct s24a8 {
            int64_t v[3];
        };
        assert_size_alignment<s24a8, 24, 8>();
        auto p4 = arena_alloc<s24a8>(&a, 1);
        testing::assertf(t, p4, "s24a8");
        testing::logf(t, "&s24a8: %p", p4);
    });
}

#ifdef HASHMAP_H
void test_hashmap(testing::handle* t)
{
    basic_arena a = {};
    basic_arena_make(&a);
    hashmap<string, int32_t, basic_arena> hm = {};
    make_hashmap(&hm, &a, hash_key<string>, string_equal);
    
    typedef struct {
        string key;
        int32_t val;
    } pair;
    static const std::vector<pair> kvs = {
        {string_from("1"), 1},
        {string_from("2"), 2},
        {string_from("3"), 4},
    };
    for (auto i = kvs.cbegin(); i != kvs.cend(); i++) {
        hashmap_put(&hm, &i->key, &i->val);
    }
    
    for (auto i = hashmap_beg(&hm); i != hashmap_end(&hm); i++) {
        printf("{%s, %d}", (*i).key->data, *((*i).val));
    }
}
#endif

int main(void)
{
    testing::main({
        new_case("defer", test_defer),
#ifdef OLD_ARENA
        new_case("alloc", test_alloc),
#endif
        new_case("test_slice_append", test_slice_append),
        new_case("test_basic_arena", test_basic_arena),
        new_case("hashmap_put", test_basic_arena),
    });
}
