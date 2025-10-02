#include "testing.h"
#include <stdio.h>

namespace testing {

void test_main(std::initializer_list<test_case> tests)
{
    auto success = true;
    for (auto beg = tests.begin(); beg != tests.end(); beg++) {
        auto h = handler_t { beg->name };
        printf("=== RUN   %s", h.name);
        beg->f(&h);
        if (!h.is_err) {
            printf("--- FAIL:  %s", h.name);
            success = false;
        }
    }
}



}