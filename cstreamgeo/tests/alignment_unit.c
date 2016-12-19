#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstreamgeo/cstreamgeo.h>

#include "test.h"

void warp_summary_printf(const warp_summary_t* warp_summary) {
    printf("Cost: %f\t\tWarp Path Length: %zu\t\t[", warp_summary->cost, warp_summary->path_length);
    for (size_t i = 0; i < warp_summary->path_length; i++) {
        printf("(%zu, %zu), ", warp_summary->index_pairs[2 * i], warp_summary->index_pairs[2 * i + 1]);
    }
    printf("]\n");
}

void align_test_small() {

    size_t a_n = 8;
    const stream_t* a = stream_create_from_list(a_n,
                                                0.0, 0.0,
                                                1.0, 0.0,
                                                2.0, 0.0,
                                                3.0, 1.0,
                                                4.0, 2.0,
                                                5.0, 3.0,
                                                5.0, 5.0,
                                                6.0, 5.0
    );
    /* o o o o o o o
     * o o o o o o o
     * o o o o o 7 8
     * o o o o o o o
     * o o o o o 6 o
     * o o o o 5 o o
     * o o o 4 o o o
     * 1 2 3 o o o o
     */

    size_t b_n = 7;
    const stream_t* b = stream_create_from_list(b_n,
                                                0.0, 0.0,
                                                2.0, 0.0,
                                                3.0, 2.0,
                                                5.0, 4.0,
                                                6.0, 4.0,
                                                6.0, 7.0,
                                                8.0, 7.0
    );
    /*
     * o o o o o o 6 o 7
     * o o o o o o o o o
     * o o o o o o o o o
     * o o o o o 4 5 o o
     * o o o o o o o o o
     * o o o 3 o o o o o
     * o o o o o o o o o
     * 1 o 2 o o o o o o
     */

    const warp_summary_t* warp_summary = full_align(a, b);
    warp_summary_printf(warp_summary);
    assert_int_equal(warp_summary->path_length, 8);
    assert_true(warp_summary->cost == 13.0);
    size_t correct[16] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 3, 5, 3, 6, 4, 7, 5};
    for (size_t i = 0; i < 2*warp_summary->path_length; i++) {
        assert_true(warp_summary->index_pairs[i] == correct[i]);
    }

    stream_destroy(a);
    stream_destroy(b);
}


int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(align_test_small),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
