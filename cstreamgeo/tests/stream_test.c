#include <stdlib.h>
#include <stdio.h>
#include <stream.h>
#include "libcut.h"


LIBCUT_TEST(compute_sparsity_evenly_spaced) {
    int a_n = 5;
    float* a = malloc(2 * a_n * sizeof(float));
    // Diagonal line
    a[0] = 0.0; a[1] = 0.0;
    a[2] = 1.0; a[3] = 1.0;
    a[4] = 2.0; a[5] = 2.0;
    a[6] = 3.0; a[7] = 3.0;
    a[8] = 4.0; a[9] = 4.0;
    float* sparsity = malloc(a_n * sizeof(float));
    stream_sparsity(a_n, a, sparsity);
    for(int i = 0; i < a_n; i++ ) {
        LIBCUT_TEST_EQ(sparsity[i], 0.5);
    }
    free(sparsity);
    free(a);

}

LIBCUT_TEST(compute_sparsity_unevenly_spaced) {
    int a_n = 5;
    float* a = malloc(2 * a_n * sizeof(float));
    // Diagonal line
    a[0] = 0.0; a[1] = 0.0;
    a[2] = 1.0; a[3] = 1.0;
    a[4] = 2.0; a[5] = 2.0;
    a[6] = 3.0; a[7] = 3.0;
    a[8] = 100.0; a[9] = 100.0;
    float* sparsity = malloc(a_n * sizeof(float));
    stream_sparsity(a_n, a, sparsity);
    LIBCUT_TEST_GT(sparsity[0], 0.974548); LIBCUT_TEST_LT(sparsity[0], 0.974550);
    LIBCUT_TEST_GT(sparsity[1], 0.974548); LIBCUT_TEST_LT(sparsity[1], 0.974550);
    LIBCUT_TEST_GT(sparsity[2], 0.974548); LIBCUT_TEST_LT(sparsity[2], 0.974550);
    LIBCUT_TEST_GT(sparsity[3], 0.300342); LIBCUT_TEST_LT(sparsity[3], 0.300344);
    LIBCUT_TEST_GT(sparsity[4], 0.160582); LIBCUT_TEST_LT(sparsity[4], 0.160584);
    free(sparsity);
    free(a);
}

LIBCUT_MAIN(
    compute_sparsity_evenly_spaced,
    compute_sparsity_unevenly_spaced
)
