#pragma once

#include <type_traits>
#include <concepts>
#include <initializer_list>
#include <stdarg.h>
#include <thread>

namespace testing {
struct handler_t; 

    typedef void (*test_func)(handler_t*);

    template <typename T>
    concept is_test_func = std::invocable<T>;

    struct handler_t {
        const char* name;
        int8_t indents;
        bool is_err;

        template <typename T> 
        requires is_test_func<T>
        void run(const char* name, T&& f);

        void logf(const char *fmt, ...);
        void assert(bool cond, const char *fmt, ...);
        // void fatalf(const char *fmt, ...);
        // void failnow();
    };

    typedef struct {
        const char* name;
        test_func   f;
    } test_case;

    void test_main(std::initializer_list<test_case> tests);
}