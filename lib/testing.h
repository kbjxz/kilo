#ifndef TESTING_H
#define TESTING_H

#include <stdlib.h>
#include <string.h>
#include <initializer_list>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <type_traits>

namespace testing {

struct handle;

typedef void (*test_func)(handle*);

struct case_t {
    const char* name;
    test_func f;
    void* args;
};

struct handle {
    handle* parent;
    case_t tc; 
    int8_t indents;
    void* args;
    bool is_pass;
};

void main(std::initializer_list<case_t> tests);
void test_setup(handle* h, const case_t* tc, handle* parent);
void test_print_name(handle* h);
bool test_run(handle* h);
void subtest(
    handle* t, 
    const char* name, 
    test_func f, 
    void* args = NULL
);
case_t new_case(const char* name, test_func f, void* args = 0);
template <typename T>
T* test_get_args(handle* t);

namespace internal {

    void print_with_tab(const handle*h, const char* fmt, ...);

    typedef std::decay_t<decltype(__FILE__)> __file_t ;
    typedef std::decay_t<decltype(__LINE__)> __line_t ;

    void __logf(
        handle* h, __file_t file, __line_t line, 
        const char* fmt, ...
    );

    void __assert(
        handle* t, __file_t file, __line_t line,  
        bool cond, const char* fmt, ...
    );

} // end of internal

#define logf(t, fmt, ...) \
internal::__logf(t, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define tassert(t, cond, fmt, ...) \
internal::__assert(t, __FILE__, __LINE__, cond, fmt __VA_OPT__(,) __VA_ARGS__)

} // end of testing

#endif