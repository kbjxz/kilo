#ifndef ERR_H
#define ERR_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <concepts>

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

void __panic(
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

void __panic_va(
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

}



#endif