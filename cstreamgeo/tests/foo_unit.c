#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstreamgeo/code_to_be_tested/foo.h>

#include <test.h>

void foo_test_1() {
    DESCRIBE_TEST;
}

void foo_test_2() {
    DESCRIBE_TEST;
}


int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(foo_test_1), 
        cmocka_unit_test(foo_test_2),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
