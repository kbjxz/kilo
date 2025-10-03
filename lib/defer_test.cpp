#include "defer.h"
#include "testing/testing.h"

void test_defer(test_handler* t)
{
    auto counter = 0;
    {
        counter++;
        defer
        {
            counter--;
        };
    }
    t->assert(counter == 0, "[counter] exp: 0, got: %d", counter);
}

int main(void)
{
    test_main({
        test_case { "defer", nullptr, test_defer },
    });
}