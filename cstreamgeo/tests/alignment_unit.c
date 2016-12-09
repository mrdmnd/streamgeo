#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstreamgeo/cstreamgeo.h>

#include "test.h"

void align_test_small() {

    size_t a_n = 8;
    const stream_t* a = stream_create_from_list(a_n,
                                                1.0, 1.0,
                                                1.0, 2.0,
                                                2.0, 3.0,
                                                3.0, 3.0,
                                                4.0, 3.0,
                                                5.0, 2.0,
                                                6.0, 4.0,
                                                4.0, 4.0
    );
    /* o o o o o o o
     * o o o o o o o
     * o o o o o o o
     * o o o o 8 o 7
     * o o 3 4 5 o o
     * o 2 o o o 6 o
     * o 1 o o o o o
     * o o o o o o o
     */

    size_t b_n = 6;
    const stream_t* b = stream_create_from_list(b_n,
                                                2.0, 1.0,
                                                1.0, 1.0,
                                                2.0, 3.0,
                                                3.0, 3.0,
                                                4.0, 4.0,
                                                3.0, 4.0
    );
    /* o o o o o o o o
     * o o o 6 5 o o o
     * o o 3 4 o o o o
     * o o o o o o o o
     * o 2 1 o o o o o
     * o o o o o o o o
     */

    float* cost = malloc(sizeof(float));
    size_t* path_length = malloc(sizeof(size_t));
    size_t* warp_path = full_align(a, b, cost, path_length);
    printf("Cost: %f Warp Path Length: %zu [", *cost, *path_length);
    for (size_t i = 0; i < *path_length; i++) {
        printf("(%zu, %zu), ", warp_path[2 * i], warp_path[2 * i + 1]);
    }
    printf("]\n");
    assert_int_equal(*path_length, 8);
    assert_true(*cost == 13.0);
    size_t correct[16] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 3, 5, 3, 6, 4, 7, 5};
    for (size_t i = 0; i < *path_length * 2; i++) {
        assert_true(warp_path[i] == correct[i]);
    }

    free(cost);
    free(path_length);
    stream_destroy(a);
    stream_destroy(b);
}


int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(align_test_small),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
