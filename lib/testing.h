#include <stdlib.h>
#include <string.h>
#include <initializer_list>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <type_traits>

struct test_handler;

typedef void (*test_func)(test_handler*);

struct test_case {
    const char* name;
    test_func f;
    void* args;
};

inline test_case new_case(
    const char* name,
    test_func f,
    void* args = 0)
{
    return test_case{
        .name = name,
        .f = f,
        .args = args,
    };
}

using __file_t = std::decay_t<decltype(__FILE__)>;
using __line_t = std::decay_t<decltype(__LINE__)>;

struct test_handler {
    test_handler* parent;
    test_case tc; 
    int8_t indents;
    void* args;
    bool is_pass;
};

void test_main(std::initializer_list<test_case> tests);
void test_setup(test_handler* h, const test_case* tc, test_handler* parent);
void test_print_name(test_handler* h);
bool test_run(test_handler* h);
void __print_with_tab(const test_handler*h, const char* fmt, ...);

template <typename T>
T* test_get_args(test_handler* t)
{
    return (T*)t->args;
}

inline void test_subtest(
    test_handler* t, 
    const char* name, 
    test_func f, 
    void* args = NULL
)
{
    test_handler subt;
    test_case tc = new_case(name, f, args);
    test_setup(&subt, &tc, t);
    bool is_pass = test_run(&subt);
    t->is_pass = (t->is_pass && is_pass);
}


inline void __print_with_tab(const test_handler* h, const char* fmt, ...)
{
    // print indents
    switch (h->indents) {
        case 0: 
        break; case 1: printf("\t");
        break; case 2: printf("\t\t");
        break; case 3: printf("\t\t\t");
        break; default:
            char* buf = (char*)malloc(h->indents+1);
            memset(buf, '\t', h->indents);
            buf[h->indents] = '\0'; 
            printf("%s", buf);
    }
    
    // print data 
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

#define test_logf(h, fmt, ...) (__logf(h, __FILE__, __LINE__, fmt, ##__VA_ARGS__))
inline void __logf(test_handler* h, __file_t file, __line_t line, const char* fmt, ...)
{
    __print_with_tab(h, "%s:%d: ", file, line);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

#define test_assert(h, cond, fmt, ...) \
__assert(h, __FILE__, __LINE__, cond, fmt, ##__VA_ARGS__)
inline void __assert(
    test_handler* t, 
    __file_t file, 
    __line_t line, 
    bool cond, 
    const char* fmt, ...
)
{
    if (cond) {
        return;
    } else {
        t->is_pass = false;
    }

    __print_with_tab(t, "%s:%d: ", file, line);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}



inline void test_main(std::initializer_list<test_case> tests)
{
    auto is_all_pass = true;
    for (auto beg = tests.begin(); beg != tests.end(); beg++) {
        test_handler h;
        test_setup(&h, beg, 0);
        is_all_pass = (is_all_pass && test_run(&h));
    }
    if (is_all_pass) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
}

inline void test_setup(test_handler* h, const test_case* tc, test_handler* parent)
{
    *h = test_handler{};
    h->is_pass = true;
    h->tc = *tc;
    h->parent = parent;
    if (h->parent) {
        h->indents = parent->indents+1;
    }
}


inline bool test_run(test_handler* h)
{
    __print_with_tab(h, "=== RUN     ");
    test_print_name(h);
    printf("\n");

    h->tc.f(h);

    if (!h->is_pass) {
        __print_with_tab(h, "--- FAIL:   ");
    } else {
        __print_with_tab(h, "--- PASS:   ");
    }
    test_print_name(h);
    printf("\n");

    return h->is_pass;
}

inline void test_print_name(test_handler* h) {
    if (h->parent) {
        test_print_name(h->parent);
        printf("/%s", h->tc.name);
    } else {
        printf("%s", h->tc.name);
    }
}
