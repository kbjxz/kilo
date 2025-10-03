#include <initializer_list>
#include <stdarg.h>
#include <stdint.h>

struct test_handler;

typedef void (*test_func)(test_handler*);

struct test_case {
    const char* name;
    void* args;
    test_func f;
};

struct test_handler {
    test_handler* parent;
    test_case tc; 
    int8_t indents;
    bool is_pass;

    void* get_args();
    void subtest(const test_case* tc);
    void logf(const char* fmt, ...);
    void assert(bool cond, const char* fmt, ...);
};

void test_main(std::initializer_list<test_case> tests);
void test_setup(test_handler* h, const test_case* tc, test_handler* parent);
void test_print_name(test_handler* h);
bool test_run(test_handler* h);

#include <stdio.h>

void test_main(std::initializer_list<test_case> tests)
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

void test_setup(test_handler* h, const test_case* tc, test_handler* parent)
{
    h->is_pass = true;
    h->tc = *tc;
    h->parent = parent;
    if (h->parent) {
        h->indents = parent->indents+1;
    }
}

bool test_run(test_handler* h)
{
    printf("=== RUN     ");
    test_print_name(h);
    printf("\n");

    h->tc.f(h);

    if (!h->is_pass) {
        printf("--- FAIL:   ");
    } else {
        printf("--- PASS:   ");
    }
    test_print_name(h);
    printf("\n");

    return h->is_pass;
}

void test_print_name(test_handler* h) {
    if (h->parent) {
        test_print_name(h->parent);
        printf("/%s", h->tc.name);
    } else {
        printf("%s", h->tc.name);
    }
}

void* test_handler::get_args() 
{
    return this->tc.args;
}

void test_handler::subtest(const test_case* tc)
{
    test_handler h;
    test_setup(&h, tc, this);
    bool is_pass = test_run(&h);
    this->is_pass = (this->is_pass && is_pass);
}

void test_handler::logf(const char* fmt, ...)
{
    printf("\t%s:%d: s", __FILE__, __LINE__);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void test_handler::assert(bool cond, const char* fmt, ...)
{
    if (cond) {
        return;
    } else {
        this->is_pass = false;
    }

    printf("\t%s:%d: s", __FILE__, __LINE__);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}
