#ifndef HASHMAP_H
#define HASHMAP_H

#include "bitmap.h"
#include "string.h"

template <typename K, typename V, typename A>
requires is_arena_allocator<A>
struct hashmap {
    typedef K key_type;
    typedef V val_type;
    typedef A arena_type;
    typedef bool(*equal_func )(const K*, const K*) ;
    typedef size_t(*hash_func)(const K*) ;
    struct kv_pair {
        const key_type* key; 
        val_type* val;
    };
    
    static constexpr int load_factor = 6; // => 0.6
    
    int32_t len;
    int32_t cap;
    hash_func hash;
    equal_func equal;
    A* arena;
    bitmap key_map;
    slice<K> keys;
    slice<V> vals;
    
    maybe<V*> operator[](const K* key)
    {
        return hashmap_get(this, key);
    }
};

template <typename K, typename V, typename _>
struct hashmap_iter {
    int32_t pos;
    hashmap<K, V, _>* hm;
    
    hashmap_iter operator++(int)
    {
        auto old = *this;
        pos++;
        for (;;) {
            if (pos >= hm->cap || bitmap_get(&hm->key_map, pos)) {
                break;
            } else {
                pos++;
            }
        }
        return old;
    }

    hashmap<K, V, _>::kv_pair operator*() 
    {
        return {&hm->keys[pos], &hm->vals[pos]};
    }
    
    bool operator==(const hashmap_iter& oth) {
        return hm == oth.hm && pos == oth.pos;
    }

    bool operator!=(const hashmap_iter& oth) {
        return !((*this) == oth);
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
    *hm = hashmap<K, V, A>{
        .len = 0,
        .cap = cap,
        .hash = hash,
        .equal = equal,
        .arena = arena,
        .key_map = BITMAP_ZERO,
        .keys = {},
        .vals = {}
    };
    slice_make_n(&hm->keys, cap, arena);
    slice_make_n(&hm->vals, cap, arena);
}

namespace internal {

typedef const char* put_result;

static put_result PUT_RESULT_INSERTED = "inserted";
static put_result PUT_RESULT_UPDATED = "updated";
static put_result PUT_RESULT_FAILED = "failed";

// linear probing
template <typename K, typename V, typename _>
put_result hashmap_put_noresize(
    hashmap<K, V, _>* hm,
    const K* key, 
    const V* val
)
{
    const int32_t cap = hm->cap;
    const auto hash_val = hm->hash(key);
    const int32_t start = hash_val % cap;
    put_result ret = PUT_RESULT_FAILED;
    int32_t i = start;
    for (; i != start + cap; i++) {
        const auto pos = i < cap ? i : i % cap;
        if (!bitmap_get(&hm->key_map, pos)) {
            slice_at(&hm->keys, pos) = *key;
            slice_at(&hm->vals, pos) = *val;
            ret = PUT_RESULT_INSERTED;
            break;
        } else if (hm->equal(key, &hm->keys[pos])) {
            slice_at(&hm->vals, pos) = *val;
            ret = PUT_RESULT_UPDATED;
            break;
        }
    }

    if (ret == internal::PUT_RESULT_INSERTED) {
        hm->len++;
        bitmap_set(&hm->key_map, i < cap ? i : i % cap);
    }

#ifndef NDEBUG
    printf("__hashmap_put_noresize: {hash_pos: %d=%lo%%%d, tried: %d}\n", start, hash_val, cap, i-start);
#endif

    return ret;
}


template <typename K, typename V, typename A>
void hashmap_resize(hashmap<K, V, A>* old)
{
    hashmap<K, V, A> result = *old;
    result.cap = old->cap * 2 + 1;
    result.keys = {};
    slice_make_n(&result.keys, result.cap, result.arena);
    result.vals = {};
    slice_make_n(&result.vals, result.cap, result.arena);
    // const int32_t new_cap = old->cap * 2 + 1;
    // slice<maybe<K>> new_keys = {};
    // slice_make_n(&new_keys, new_cap, old->arena);
    // slice<V> new_vals = {};
    // slice_make_n(&new_vals, new_cap, old->arena);
    
    for (int32_t i = 0; i < old->cap; i++) {
        if (!bitmap_get(&old->key_map, i)) {
            continue;
        }
        
        auto put_result = hashmap_put_noresize<K, V, A>(
            &result, &old->keys[i], &old->vals[i]);
        assert(put_result != PUT_RESULT_FAILED, "move kv failed");
    }
    
    *old = result;    
}
} // end of internal

template <typename K, typename V, typename A>
void hashmap_put(hashmap<K, V, A>* hm, const K* key, const V* val)
{
    if (hm->len * 10 / hm->cap >= hm->load_factor) {
        internal::hashmap_resize(hm);
    }

    auto put_result = internal::hashmap_put_noresize<K, V, A>(hm, key, val);
    assert(put_result != internal::PUT_RESULT_FAILED, "put failed");
}

template <typename K, typename V, typename _>
maybe<V*> hashmap_get(const hashmap<K, V, _>* hm, const K* key)
{
    const int32_t beg = hm->hash(key) % hm->cap;
    const int32_t end = beg + hm->cap;
    for (auto i = beg; i != end; i++) {
        auto mkey = hm->keys[i];
        if (mkey && equal(key, &mkey->val)) {
            return some(hm->vals[i]);
        }
    }
    return none<V*>();
}
    
template <typename K, typename V, typename _>
hashmap_iter<K, V, _> hashmap_beg(hashmap<K, V, _>* hm)
{
    if (hm->len == 0) {
        return hashmap_end(hm);
    }

    int32_t pos = 0;
    for (;;) {
        assert(pos != hm->cap, "there must be at least 1 kv-pair given len=%d", hm->len);
        if (bitmap_get(&hm->key_map, pos)) {
#ifndef NDEBUG
        printf("hashmap_beg: key_map(%d)=true\n", pos);
#endif
            break;
        }
#ifndef NDEBUG
        printf("hashmap_beg: key_map(%d)=false; ", pos);
#endif
        pos++;
    }
    return hashmap_iter{.pos = pos, .hm = hm};
}

template <typename K, typename V, typename _>
hashmap_iter<K, V, _> hashmap_end(hashmap<K, V, _>* hm)
{
    return hashmap_iter{.pos = hm->cap, .hm = hm};
}

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
inline uint64_t hash_bytes(const void* key, size_t n) {

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

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
    return hash_bytes(key, sizeof(T));
}

template <typename T>
uint64_t hash_key(const slice<T>* key)
{
    return hash_bytes(key->data, key->len);
}

#endif