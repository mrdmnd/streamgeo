#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstreamgeo.h>

#include "test.h"

void create_from_list_test() {
    size_t a_n = 3;
    stream_t stream;
    stream_init_from_list(&stream, a_n, POSITION, 1.0, 5.0, 2.0, 2.0, 3.0, 3.0);
    assert_true(stream.stream_contents.position_stream.points[0].lat = 1.0);
    assert_true(stream.stream_contents.position_stream.points[0].lng = 5.0);
    stream_release(&stream);
}

void compute_stream_distance_test() {
    size_t a_n = 5;
    stream_t stream;
    stream_init_from_list(&stream, a_n, POSITION, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 4.0, 4.0);
    stream_printf(&stream);
    float distance = stream_distance(&stream.stream_contents.position_stream);
    assert_true(5.656853 < distance);
    assert_true(distance < 5.656855);
    stream_release(&stream);
}


void compute_sparsity_evenly_spaced_test() {
    size_t a_n = 5;
    stream_t stream;
    stream_init_from_list(&stream, a_n, POSITION, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 4.0, 4.0);
    float* sparsity = malloc(a_n * sizeof(float));
    stream_sparsity_init(&stream.stream_contents.position_stream, sparsity);
    for (size_t i = 0; i < a_n; i++) {
        assert_true(sparsity[i] == 0.5f);
    }
    free(sparsity);
    stream_release(&stream);

}

void compute_sparsity_unevenly_spaced_test() {
    size_t a_n = 5;
    stream_t stream;
    stream_init_from_list(&stream, a_n, POSITION, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 100.0, 100.0);
    float* sparsity = malloc(a_n * sizeof(float));
    stream_sparsity_init(&stream.stream_contents.position_stream, sparsity);
    assert_true(0.974548 < sparsity[0]);
    assert_true(sparsity[0] < 0.974550);
    assert_true(0.974548 < sparsity[1]);
    assert_true(sparsity[1] < 0.974550);
    assert_true(0.974548 < sparsity[2]);
    assert_true(sparsity[2] < 0.974550);
    assert_true(0.300342 < sparsity[3]);
    assert_true(sparsity[3] < 0.300344);
    assert_true(0.160582 < sparsity[4]);
    assert_true(sparsity[4] < 0.160584);
    free(sparsity);
    stream_release(&stream);
}

void compute_ramer_douglas_peucker_test_small() {
    size_t a_n = 7;
    stream_t stream;
    stream_init_from_list(&stream, a_n, POSITION,
                                                     0.0, 0.0,
                                                     1.0, 0.0,
                                                     3.0, 0.0,
                                                     5.0, 10.0,
                                                     8.0, 0.0,
                                                     10.0, 0.0,
                                                     12.0, 0.0
    );
    position_stream_t underlying_stream = stream.stream_contents.position_stream;
    downsample_rdp(&underlying_stream, 1.5);
    assert_int_equal(underlying_stream.n, 5);
    float correct[10] = { 0.0, 0.0, 3.0, 0.0, 5.0, 10.0, 8.0, 0.0, 12.0, 0.0};
    for (size_t i = 0; i < underlying_stream.n; i++) {
        assert_true(underlying_stream.points[i].lat == correct[2*i]);
        assert_true(underlying_stream.points[i].lng == correct[2*i+1]);
    }
    stream_release(&stream);
}

void compute_ramer_douglas_peucker_test_medium() {
    size_t a_n = 10;
    // . . . . . . . . X . . .
    // . . . X . . . . . . . X
    // . . . . . . . . . . X .
    // X o X . X o X . . . . .
    // should lose the second and sixth points
    stream_t stream;
    stream_init_from_list(&stream, a_n, POSITION,
                                                     0.0, 0.0,
                                                     1.0, 0.0,// disappears
                                                     2.0, 0.0,
                                                     3.0, 2.0,
                                                     4.0, 0.0,
                                                     5.0, 0.0, // disappears
                                                     6.0, 0.0,
                                                     8.0, 3.0,
                                                     10.0, 1.0,
                                                     11.0, 2.0
    );
    position_stream_t underlying_stream = stream.stream_contents.position_stream;
    downsample_rdp(&underlying_stream, 1.1);
    assert_int_equal(underlying_stream.n, 8);
    float correct[16] = {0.0, 0.0, 2.0, 0.0, 3.0, 2.0, 4.0, 0.0, 6.0, 0.0, 8.0, 3.0, 10.0, 1.0, 11.0, 2.0};
    for (size_t i = 0; i < underlying_stream.n; i++) {
        assert_true(underlying_stream.points[i].lat == correct[2*i]);
        assert_true(underlying_stream.points[i].lng == correct[2*i+1]);
    }
    stream_release(&stream);
}

void compute_ramer_douglas_peucker_test_duplicates() {
    size_t a_n = 4;
    // . . . . .
    // . . X . .  <-- point is stacked up twice here
    // . . . . .
    // X . . . X
    // should lose the point at index 1 or 2, doesn't matter which
    stream_t stream;
    stream_init_from_list(&stream, a_n, POSITION,
                                                     0.0, 0.0,
                                                     2.0, 2.0,// disappears
                                                     2.0, 2.0,
                                                     4.0, 0.0
    );
    position_stream_t underlying_stream = stream.stream_contents.position_stream;
    downsample_rdp(&underlying_stream, 1.1);
    assert_int_equal(underlying_stream.n, 3);
    float correct[6] = {0.0, 0.0, 2.0, 2.0, 4.0, 0.0};
    for (size_t i = 0; i < underlying_stream.n; i++) {
        assert_true(underlying_stream.points[i].lat == correct[2*i]);
        assert_true(underlying_stream.points[i].lng == correct[2*i+1]);
    }
    stream_release(&stream);
}

int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(create_from_list_test),
            cmocka_unit_test(compute_stream_distance_test),
            cmocka_unit_test(compute_sparsity_evenly_spaced_test),
            cmocka_unit_test(compute_sparsity_unevenly_spaced_test),
            cmocka_unit_test(compute_ramer_douglas_peucker_test_small),
            cmocka_unit_test(compute_ramer_douglas_peucker_test_medium),
            cmocka_unit_test(compute_ramer_douglas_peucker_test_duplicates)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
