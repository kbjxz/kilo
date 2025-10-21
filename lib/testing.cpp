#include "testing.h"
#include "bitmap.h"
#include <cstdlib>

namespace testing {


case_t new_case(
    const char* name,
    test_func f,
    void* args
)
{
    return case_t{
        .name = name,
        .f = f,
        .args = args,
    };
}

template <typename T>
T* test_get_args(handle* t)
{
    return (T*)t->args;
}

void subtest(
    handle* t, 
    const char* name, 
    test_func f, 
    void* args
)
{
    handle subt;
    case_t tc = new_case(name, f, args);
    test_setup(&subt, &tc, t);
    bool is_pass = test_run(&subt);
    t->is_pass = (t->is_pass && is_pass);
}

namespace internal {

    void print_with_tab(const handle* h, const char* fmt, ...)
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
    
    using internal::__file_t;
    using internal::__line_t;

    void __logf(handle* h, __file_t file, __line_t line, const char* fmt, ...)
    {
        print_with_tab(h, "%s:%d: ", file, line);
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }

    void __assert(
        handle* t, 
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

        print_with_tab(t, "%s:%d: ", file, line);

        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }

    struct test_state_t {
        int64_t test_cases_count;
        bitmap pass_results;
    };

    test_state_t* get_test_state()
    {
        static test_state_t state = {};
        return &state;
    }

    void on_exit()
    {
        auto state = get_test_state();
        if (bitmap_and(&state->pass_results, 0, state->test_cases_count)) {
            printf("PASS\n");
        } else {
            printf("FAIL\n");
        }
    }
} // end of intenral


void main(std::initializer_list<case_t> tests)
{
    std::atexit(internal::on_exit);
    auto state = internal::get_test_state();
    for (size_t i = 0; i < tests.size(); i++) {
        handle h = {};
        test_setup(&h, tests.begin()+i, 0);
        const bool pass = test_run(&h);
        if (pass) {
            bitmap_set(&state->pass_results, i);
        }
    }
}

void test_setup(handle* h, const case_t* tc, handle* parent)
{
    *h = handle{};
    h->is_pass = true;
    h->tc = *tc;
    h->parent = parent;
    if (h->parent) {
        h->indents = parent->indents+1;
    }
}

bool test_run(handle* h)
{
    internal::print_with_tab(h, "=== RUN     ");
    test_print_name(h);
    printf("\n");

    h->tc.f(h);

    if (!h->is_pass) {
        internal::print_with_tab(h, "--- FAIL:   ");
    } else {
        internal::print_with_tab(h, "--- PASS:   ");
    }
    test_print_name(h);
    printf("\n");

    return h->is_pass;
}

void test_print_name(handle* h) {
    if (h->parent) {
        test_print_name(h->parent);
        printf("/%s", h->tc.name);
    } else {
        printf("%s", h->tc.name);
    }
}

} // end of namespace testing