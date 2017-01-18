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

void full_align_test_small() {
    const size_t a_n = 4;
    const size_t b_n = 3;
    const stream_t* a = stream_create_from_list(a_n, 0.0, 0.0, 2.0, 4.0, 4.0, 4.0, 6.0, 0.0);
    const stream_t* b = stream_create_from_list(b_n, 1.0, 0.0, 3.0, 3.5, 5.0, 0.0);
    const warp_summary_t* warp_summary = full_warp_summary_create(a, b);
    warp_summary_printf(warp_summary);

    assert_int_equal(warp_summary->path_length, 4);

    assert_true(warp_summary->cost == 4.5000);

    size_t correct[8] = {0, 0, 1, 1, 2, 1, 3, 2};
    for (size_t i = 0; i < 2*warp_summary->path_length; i++) {
        assert_true(warp_summary->index_pairs[i] == correct[i]);
    }

    free(warp_summary->index_pairs);
    free((void*) warp_summary);
    stream_destroy(a);
    stream_destroy(b);
}

void fast_align_test_small() {
    const size_t a_n = 4;
    const size_t b_n = 3;
    const size_t radius = 4;
    const stream_t* a = stream_create_from_list(a_n, 0.0, 0.0, 2.0, 4.0, 4.0, 4.0, 6.0, 0.0);
    const stream_t* b = stream_create_from_list(b_n, 1.0, 0.0, 3.0, 3.5, 5.0, 0.0);
    const warp_summary_t* warp_summary = fast_warp_summary_create(a, b, radius);
    warp_summary_printf(warp_summary);

    assert_int_equal(warp_summary->path_length, 4);

    assert_true(warp_summary->cost == 4.5000);

    size_t correct[8] = {0, 0, 1, 1, 2, 1, 3, 2};
    for (size_t i = 0; i < 2*warp_summary->path_length; i++) {
        assert_true(warp_summary->index_pairs[i] == correct[i]);
    }

    free(warp_summary->index_pairs);
    free((void*) warp_summary);
    stream_destroy(a);
    stream_destroy(b);
}


int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(full_align_test_small),
            cmocka_unit_test(fast_align_test_small),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
