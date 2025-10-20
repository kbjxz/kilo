#ifndef DEFER_H
#define DEFER_H

#include <type_traits>
 
#define __DEFER__(V)  __defer const V = [&](void)->void
#define defer __DEFER(__COUNTER__)
#define __DEFER(N) __DEFER_(N)
#define __DEFER_(N) __DEFER__(__DEFER_VARIABLE_ ## N)

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

#endif