#include <concepts>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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
 
#define __DEFER__(V)  __defer const V = [&](void)->void


#define defer __DEFER(__COUNTER__)
#define __DEFER(N) __DEFER_(N)
#define __DEFER_(N) __DEFER__(__DEFER_VARIABLE_ ## N)

inline void panic(const char* s)
{
    perror(s);
    exit(1);
}

template <typename F>
requires std::is_invocable_v<F>
void panic(const char* s, F cleanup)
{
    cleanup();
    perror(s);
    exit(1);
}

template <typename T, typename Err = int>
struct result {
    Err error;
    T value;
};

inline void assert(bool cond, const char* fmt, ...)
{
#ifndef NDEBUG
    if (cond) {
        return;
    } 

    printf("\t%s:%d: s", __FILE__, __LINE__);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
#endif
}