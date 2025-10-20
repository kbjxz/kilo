#ifndef HASHMAP_H
#define HASHMAP_H

#include "slice.h"
#include "string.h"

template <typename K, typename V, typename A>
requires is_arena_allocator<A>
struct hashmap {
    typedef K key_type;
    typedef slice<maybe<key_type>> key_slice;
    typedef V val_type;
    typedef slice<V> val_slice;
    typedef A arena_type;
    typedef bool(*equal_func )(const K*, const K*) ;
    typedef  size_t(*hash_func)(const K*) ;
    typedef struct {
        const key_type* key; 
        val_type* val;
    } kv_pair;
    
    static constexpr int load_factor = 6; // => 0.6
    
    int32_t len;
    int32_t cap;
    hash_func hash;
    equal_func equal;
    A* arena;
    slice<maybe<K>> keys;
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
        for (; pos != hm->cap && !hm->keys[pos]; pos++) {
        }
        return old;
    }
    
    

    hashmap<K, V, _>::kv_pair operator*() 
    {
        return {&hm->keys[pos].value, &hm->vals[pos]};
    }
    
    bool operator==(const hashmap_iter& oth) {
        return hm == oth.hm && pos == oth.pos;
    }
    bool operator!=(const hashmap_iter& oth) {
        return (*this) != oth;
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
    typename hashmap<K, V, _>::key_slice* keys, 
    typename hashmap<K, V, _>::val_slice* vals,
    const typename hashmap<K, V, _>::key_type* key, 
    const typename hashmap<K, V, _>::val_type* val,
    typename hashmap<K, V, _>::hash_func hash,
    typename hashmap<K, V, _>::equal_func equal
)
{
    const int32_t cap = keys->cap;
    int32_t pos = hash(key) % cap;
    for (auto i = pos; i != pos + cap; i++) {
        maybe<K>& mkey = keys[i%cap];
        if (!mkey) {
            keys[i] = some(*key);
            vals[i] = *val;
            return true;
        } else if (equal(key, &mkey.val)) {
            V& v = vals[i];
            v = *val;
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
        
        auto ok = __hashmap_put_noresize<K, V, A>(
            &new_keys, &new_vals, 
            &hm->keys[i].value, &hm->vals[i], 
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

    auto ok =__hashmap_put_noresize<K, V, A>(
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
static uint64_t hash_bytes(const void* key, size_t n) {
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