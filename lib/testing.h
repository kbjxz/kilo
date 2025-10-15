#include <stdlib.h>
#include <string.h>
#include <initializer_list>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

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

struct test_handler {
    test_handler* parent;
    test_case tc; 
    int8_t indents;
    bool is_pass;

    void* get_args();
    void subtest(const char* name, test_func f, void* args = NULL);
    void logf(const char* fmt, ...);
    void assert(bool cond, const char* fmt, ...);
};

void test_main(std::initializer_list<test_case> tests);
void test_setup(test_handler* h, const test_case* tc, test_handler* parent);
void test_print_name(test_handler* h);
bool test_run(test_handler* h);
void tprint(const test_handler*h, const char* fmt, ...);

#include <stdio.h>

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

inline void tprint(const test_handler* h, const char* fmt, ...)
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

inline bool test_run(test_handler* h)
{
    tprint(h, "=== RUN     ");
    test_print_name(h);
    printf("\n");

    h->tc.f(h);

    if (!h->is_pass) {
        tprint(h, "--- FAIL:   ");
    } else {
        tprint(h, "--- PASS:   ");
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

inline void* test_handler::get_args() 
{
    return this->tc.args;
}

inline void test_handler::subtest(
    const char* name,
    test_func f,
    void* args)
{
    test_handler h;
    test_case tc = new_case(name, f, args);
    test_setup(&h, &tc, this);
    bool is_pass = test_run(&h);
    this->is_pass = (this->is_pass && is_pass);
}

inline void test_handler::logf(const char* fmt, ...)
{
    tprint(this, "%s:%d: ", __FILE__, __LINE__);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

inline void test_handler::assert(bool cond, const char* fmt, ...)
{
    if (cond) {
        return;
    } else {
        this->is_pass = false;
    }

    tprint(this, "%s:%d: ", __FILE__, __LINE__);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}
