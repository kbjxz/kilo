#ifndef ERR_H
#define ERR_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>

inline void panic_errno(const char* s)
{
    perror(s);
    exit(1);
}

/* inline void panic(void)
{
    exit(1);
} */

#define panic(fmt, ...)\
internal::__panic_va(__FILE__, __LINE__ , fmt __VA_OPT__(,) __VA_ARGS__)

#define assert(cond, fmt, ...)\
internal::__assert(__FILE__, __LINE__, cond, fmt __VA_OPT__(,) __VA_ARGS__)

namespace internal {

typedef std::decay_t<decltype(__FILE__)> file_t;
typedef std::decay_t<decltype(__LINE__)> line_t;

inline void __panic(
    file_t file,
    line_t line,
    const char* fmt, 
    va_list args
)
{
    fprintf(stderr, "panic: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n\t%s:%d\n", file, line);
    exit(1);
}

inline void __panic_va(
    file_t file,
    line_t line,
    const char* fmt, 
    ...
)
{
    va_list args;
    va_start(args, fmt);
    __panic(file, line, fmt, args);
    va_end(args);
}

inline void __assert(
    std::decay_t<decltype(__FILE__)> file,
    std::decay_t<decltype(__LINE__)> line,
    bool cond, const char* fmt, ...
)
{
    if (cond) {
        return;
    } 


    va_list args;
    va_start(args, fmt);
    __panic(file, line, fmt, args);
    va_end(args);
}

} // end of namespace internal

template <typename T>
struct maybe {
    T val;
    bool ok;
    
    operator bool()
    {
        return ok; 
    }
};

template <typename T>
maybe<T> some(T v)
{
    return maybe<T>{.val = v, .ok = true};
}

template <typename T>
maybe<T> none()
{
    return maybe<T>{.val = {}, .ok = false};
}

typedef const char* error;

template <typename T, typename Err = error>
struct result {
    maybe<Err> merr;
    T val;
    
    operator bool ()
    {
        return !merr;
    }
};

template <typename T, typename Err>
result<T, Err> result_err(Err e)
{
    return result<T, Err>{.merr = some(e), .val ={}};
}

template <typename T, typename Err>
result<T, Err> result_val(T v)
{
    return result<T, Err>{.merr = none<Err>(), .val =v};
}

template <typename _, typename Err>
Err result_get_err(const result<_, Err>& r)
{
    return r.merr.val;
}

#endif