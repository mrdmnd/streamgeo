#include <stdlib.h>
#include <stdio.h>
#include <similarity.h>
#include <time.h>
#include "libcut.h"

void align_perf_test(int u_n, int v_n, int radius, int n_times) {
    double time_accum = 0;
    for (int j = 0; j < n_times; j++) {
        srand(time(NULL));

        float* u = malloc(2 * u_n * sizeof(float));
        float* v = malloc(2 * v_n * sizeof(float));
        // Fill streams with some garbage values that are correlated
        for (int i = 0; i < u_n; ++i) {
            u[2 * i] = (float) (i * 1.0 * rand() / RAND_MAX);
            u[2 * i + 1] = (float) (i * 1.0 * rand() / RAND_MAX);
        }
        for (int i = 0; i < v_n; ++i) {
            v[2 * i] = (float) (i * 1.0 * rand() / RAND_MAX);
            v[2 * i + 1] = (float) (i * 1.0 * rand() / RAND_MAX);
        }
        int* path_length = malloc(sizeof(int));
        int* path = malloc(2 * (u_n + v_n - 1) * sizeof(int));

        clock_t start, end;
        start = clock();
        align(u_n, u, v_n, v, radius, path_length, path);
        end = clock();
        time_accum += ((double) (end - start)) / CLOCKS_PER_SEC;
        free(path_length);
        free(path);
        free(u);
        free(v);
    }
    printf("%d iterations of %d by %d fast time warp with path took %f millis, average=%f\n", n_times, u_n, v_n,
           1000.0 * time_accum, (1000.0 * time_accum) / n_times);
}

void similarity_perf_test(int u_n, int v_n, int radius, int n_times) {
    double time_accum = 0;
    for (int j = 0; j < n_times; j++) {
        srand(time(NULL));

        float* u = malloc(2 * u_n * sizeof(float));
        float* v = malloc(2 * v_n * sizeof(float));
        // Fill streams with some garbage values that are correlated
        for (int i = 0; i < u_n; ++i) {
            u[2 * i] = (float) (i * 1.0 * rand() / RAND_MAX);
            u[2 * i + 1] = (float) (i * 1.0 * rand() / RAND_MAX);
        }
        for (int i = 0; i < v_n; ++i) {
            v[2 * i] = (float) (i * 1.0 * rand() / RAND_MAX);
            v[2 * i + 1] = (float) (i * 1.0 * rand() / RAND_MAX);
        }

        clock_t start, end;
        start = clock();
        similarity(u_n, u, v_n, v, radius);
        end = clock();
        time_accum += ((double) (end - start)) / CLOCKS_PER_SEC;
        free(u);
        free(v);
    }
    printf("%d iterations of %d by %d similarity computation took %f millis, average=%f\n", n_times, u_n, v_n,
           1000.0 * time_accum, (1000.0 * time_accum) / n_times);
}

LIBCUT_TEST (align_test_small) {
    int a_n = 8;
    float* a = malloc(2 * a_n * sizeof(float));
    int b_n = 6;
    float* b = malloc(2 * b_n * sizeof(float));

    a[0] = 1.0;
    a[1] = 1.0;
    a[2] = 1.0;
    a[3] = 2.0;
    a[4] = 2.0;
    a[5] = 3.0;
    a[6] = 3.0;
    a[7] = 3.0;
    a[8] = 4.0;
    a[9] = 3.0;
    a[10] = 5.0;
    a[11] = 2.0;
    a[12] = 6.0;
    a[13] = 4.0;
    a[14] = 4.0;
    a[15] = 4.0;

    /* o o o o o o o
     * o o o o o o o
     * o o o o o o o
     * o o o o X o X
     * o o X X X o o
     * o X o o o X o
     * o X o o o o o
     * o o o o o o o
     */

    b[0] = 2.0;
    b[1] = 1.0;
    b[2] = 1.0;
    b[3] = 1.0;
    b[4] = 2.0;
    b[5] = 3.0;
    b[6] = 3.0;
    b[7] = 3.0;
    b[8] = 4.0;
    b[9] = 4.0;
    b[10] = 3.0;
    b[11] = 4.0;

    /* o o o o o o o o
     * o o o X X o o o
     * o o X X o o o o
     * o o o o o o o o
     * o X X o o o o o
     * o o o o o o o o
     */

    int* path_length = malloc(sizeof(int));
    int* path = malloc(2 * (a_n + b_n - 1) * sizeof(int));
    int radius = 1;
    float cost = align(a_n, a, b_n, b, radius, path_length, path);
    free(a);
    free(b);
    // Check the path, check the cost
    // Last element should be (7, 5) == 7 * b->n_points + 5 = 7 * 6 + 5 = 47
    //
    // 1 0 0 0 0 0
    // 0 1 0 0 0 0
    // 0 0 1 0 0 0
    // 0 0 0 1 0 0
    // 0 0 0 1 0 0
    // 0 0 0 1 0 0
    // 0 0 0 0 1 0
    // 0 0 0 0 0 1
    //
    LIBCUT_TEST_EQ(cost, 13);
    LIBCUT_TEST_EQ(*path_length, 8);

    int correct[16] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 3, 5, 3, 6, 4, 7, 5};
    for (int i = 0; i < *path_length * 2; i++) {
        LIBCUT_TEST_EQ(path[i], correct[i]);
    }
    free(path_length);
    free(path);
}

LIBCUT_TEST (similarity_test_small) {
    int a_n = 8;
    float* a = malloc(2 * a_n * sizeof(float));
    int b_n = 6;
    float* b = malloc(2 * b_n * sizeof(float));


    a[0] = 2.0;
    a[1] = 1.0;
    a[2] = 1.0;
    a[3] = 2.0;
    a[4] = 2.0;
    a[5] = 3.0;
    a[6] = 2.5;
    a[7] = 3.0;
    a[8] = 3.0;
    a[9] = 3.0;
    a[10] = 5.0;
    a[11] = 4.0;
    a[12] = 4.0;
    a[13] = 4.0;
    a[14] = 3.0;
    a[15] = 4.0;

    b[0] = 2.0;
    b[1] = 1.0;
    b[2] = 1.0;
    b[3] = 1.0;
    b[4] = 2.0;
    b[5] = 3.0;
    b[6] = 3.0;
    b[7] = 3.0;
    b[8] = 4.0;
    b[9] = 4.0;
    b[10] = 3.0;
    b[11] = 4.0;

    int radius = 1;
    float similar = similarity(a_n, a, b_n, b, radius);
    LIBCUT_TEST_GT(similar, 0.994446);
    LIBCUT_TEST_LT(similar, 0.994448);
    free(a);
    free(b);
}

LIBCUT_TEST (similarity_test_exact) {
    int a_n = 6;
    float* a = malloc(2 * a_n * sizeof(float));
    int b_n = 6;
    float* b = malloc(2 * b_n * sizeof(float));


    a[0] = 1.0;
    a[1] = 1.0;
    a[2] = 2.0;
    a[3] = 2.0;
    a[4] = 3.0;
    a[5] = 3.0;
    a[6] = 4.0;
    a[7] = 4.0;
    a[8] = 5.0;
    a[9] = 5.0;
    a[10] = 6.0;
    a[11] = 6.0;

    b[0] = 1.0;
    b[1] = 1.0;
    b[2] = 2.0;
    b[3] = 2.0;
    b[4] = 3.0;
    b[5] = 3.0;
    b[6] = 4.0;
    b[7] = 4.0;
    b[8] = 5.0;
    b[9] = 5.0;
    b[10] = 6.0;
    b[11] = 6.0;

    int radius = 1;
    float similar = similarity(a_n, a, b_n, b, radius);
    LIBCUT_TEST_EQ(similar, 1.0);
    free(a);
    free(b);
}

LIBCUT_TEST (align_bench) {
    int radius = 1;
    for (int i = 100; i < 1000; i += 10) {
        align_perf_test(i, i, radius, 30);
    }
    for (int i = 1000; i <= 3000; i += 100) {
        align_perf_test(i, i, radius, 30);
    }
}

LIBCUT_TEST (similarity_bench) {
    int radius = 1;
    for (int i = 100; i < 1000; i += 10) {
        similarity_perf_test(i, i, radius, 30);
    }
    for (int i = 1000; i <= 3000; i += 100) {
        similarity_perf_test(i, i, radius, 30);
    }
}

LIBCUT_MAIN(
        align_test_small,
        similarity_test_small,
        similarity_test_exact,
        align_bench,
        similarity_bench
)
