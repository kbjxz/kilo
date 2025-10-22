#ifndef STRING_H
#define STRING_H

#include "slice.h"

using string = slice<char>;

const string string_from(const char* a)
{
    const auto len = strlen(a);
    auto ret = string{
        .data = (char*)((void*)(&a[0])),
        .len  = len - 1,
        .cap  = len - 1,
    };

#ifndef NDEBUG
    std::cout << "string_from(\"" << a << "\"): " << ret.data << std::endl;
#endif

    return ret;
}

template <std::size_t N>
const string string_from(const char (&a)[N])
{
    auto ret = string{
        .data = (char*)((void*)(&a[0])),
        .len  = N - 1,
        .cap  = N - 1,
    };

    return ret;
}

template <typename A>
inline void string_reserve(string* s, size_t new_cap, A* a)
{
    slice_reserve(s, new_cap, a);
}

template <typename A>
void string_append(string* s, const char* v, size_t n, A* a)
{
    slice_reserve(s, s->len + n, a);
    memcpy(&s->data[s->len], v, n);
    s->len += n;
}

template <typename A>
void string_append(string* s, char c, A* a)
{
    slice_append(s, c, a);
}

template <typename A>
void string_append(string* s, const string* oth, A* a)
{
    slice_reserve(s, s->len + oth->len, a);
    memcpy(&s->data[s->len], oth->data, oth->len);  
    s->len += oth->len;
}

inline bool string_equal(const string* s1, const string* s2)
{
    if (s1->len != s2->len) {
        return false;
    }
    return memcmp(s1->data, s2->data, s1->len) == 0;
}



#endif