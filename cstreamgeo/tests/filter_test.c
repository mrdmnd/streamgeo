#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "filters.h"
#include "libcut.h"

void geometric_medoid_bench(int n_points, int iters) {
    double time_accum = 0;
    for(int j = 0; j < iters; j++) {
        srand(time(NULL));
        float* a = malloc(2 * n_points * sizeof(float));
        for(int i = 0; i < 2*n_points; i++) {
            a[i] = rand() / RAND_MAX;
        }
        clock_t start, end;
        start = clock();
        geometric_medoid(0, n_points, a);
        end = clock();
        time_accum += ((double) (end - start)) / CLOCKS_PER_SEC;
        free(a);
    }
    printf("%d iterations of geometric_medoid on window of size %d took %f millis, average=%f\n", iters, n_points, 1000.0*time_accum, (1000.0*time_accum) / iters);
}



LIBCUT_TEST(geometric_medoid_small) {
    int a_n = 7;
    float* a = malloc(2 * a_n * sizeof(float));
    // . . . . . . .
    // . . . X . . .
    // . . . . . . .
    // X X . O . X X
    a[0] = 0.0; a[1] = 0.0;
    a[2] = 1.0; a[3] = 0.0;
    a[4] = 3.0; a[5] = 0.0; // the medoid
    a[6] = 3.0; a[7] = 2.0;
    a[10] = 5.0; a[11] = 0.0;
    a[12] = 6.0; a[13] = 0.0;

    // Look at the window of size seven. Find the point from the set that minimizes the sum of squared distances
    int best_index = geometric_medoid(0, a_n, a);

    LIBCUT_TEST_EQ(best_index, 2);
    free(a);
}

LIBCUT_TEST(median_filter_small) {
    int a_n = 10;
    float* a = malloc(2 * a_n * sizeof(float));
    /*
     * . . . . . . . . . .
     * . . . . . . . . . .
     * . . . . . . . X . .
     * . . . . . . . . . .
     * . . . X . . . . . .
     * . . . . . . . . . .
     * . . . . . . . . . .
     * . . . . . . . . X .
     * . X X . X X X . . .
     * X . . . . . . . . X
     */
    a[0] = 0.0; a[1] = 0.0;
    a[2] = 1.0; a[3] = 1.0;
    a[4] = 2.0; a[5] = 1.0;
    a[6] = 3.0; a[7] = 5.0; // spike
    a[8] = 4.0; a[9] = 1.0;
    a[10] = 5.0; a[11] = 1.0;
    a[12] = 6.0; a[13] = 1.0;
    a[14] = 7.0; a[15] = 7.0; // spike
    a[16] = 8.0; a[17] = 2.0;
    a[18] = 9.0; a[19] = 0.0;

    float* output = malloc(2 * a_n * sizeof(float));
    int window_size = 3;
    median_filter(a_n, a, window_size, output);
    // Look at the window of size three. Find the point from the set that minimizes the sum of squared distances
    /*
     * . . . . . . . . . .
     * . . . . . . . . . .
     * . . . . . . . X . .
     * . . . . . . . . . .
     * . . . X . . . . . .
     * . . . . . . . . . .
     * . . . . . . . . . .
     * . . . . . . . . X .
     * . X X . X X X . . .
     * X . . . . . . . . X
     */
    // (2, 1) is duplicated!? (4, 1) is duplicated!?
    // output should be ramer douglas peucker'd
    float correct[20] = {0.0, 0.0, 1.0, 1.0, 2.0, 1.0, 2.0, 1.0, 4.0, 1.0, 5.0, 1.0, 6.0, 1.0, 8.0, 2.0, 8.0, 2.0, 9.0, 0.0};
    for(int i = 0; i < a_n; i++) {
        LIBCUT_TEST_EQ(output[2*i], correct[2*i]);
        LIBCUT_TEST_EQ(output[2*i+1], correct[2*i+1]);
    }

    free(a);
    free(output);
}

void rdp_perf_test(int u_n, float epsilon, int n_times) {
    double time_accum = 0;
    int size_accum = 0;
    for (int j = 0; j < n_times; j++) {
        float* u = malloc(2 * u_n * sizeof(float));
        for (int i = 0; i < u_n; ++i) {
            u[2*i] = (float) (i / 1000.0 );
            u[2*i+1] = (float) sin( i / 1000.0 );
        }
        int* v_n = malloc(sizeof(int));
        float* v = malloc(2 * u_n * sizeof(float));
        clock_t start, end;
        start = clock();
        reduce_by_rdp(u_n, u, epsilon, v_n, v);
        end = clock();
        time_accum += ((double) (end - start)) / CLOCKS_PER_SEC;
        size_accum += *v_n;
        free(v_n);
        free(v);
        free(u);
    }
    printf("%d iterations of RDP reduction (eps = %f) on input of size %d averaged %f millis, new size average %f\n", n_times, epsilon, u_n, (1000.0*time_accum) / n_times, 1.0*size_accum / n_times);
}

LIBCUT_TEST(reduce_by_rdp_small) {
    int a_n = 7;
    float* a = malloc(2 * a_n * sizeof(float));
    // . . . . . . .
    // . . . X . . .
    // . . . . . . .
    // X o X . X o X
    // should lose the second and sixth points
    a[0] = 0.0; a[1] = 0.0;
    a[2] = 1.0; a[3] = 0.0; // disappears
    a[4] = 2.0; a[5] = 0.0;
    a[6] = 3.0; a[7] = 2.0;
    a[8] = 4.0; a[9] = 0.0;
    a[10] = 5.0; a[11] = 0.0; // disappears
    a[12] = 6.0; a[13] = 0.0;

    int* output_n = malloc(sizeof(int));
    float* output = malloc(2 * a_n * sizeof(float));

    float epsilon = 1.1;
    reduce_by_rdp(a_n, a, epsilon, output_n, output);

    LIBCUT_TEST_EQ(*output_n, 5);
    float correct[10] = { 0.0, 0.0,  2.0, 0.0,  3.0, 2.0,  4.0, 0.0,  6.0, 0.0 };
    for(int i = 0; i < *output_n * 2; i++) {
        LIBCUT_TEST_EQ(output[i], correct[i]);
    }
    free(a);
    free(output_n);
    free(output);
}

LIBCUT_TEST(reduce_by_rdp_medium) {
    int a_n = 10;
    float* a = malloc(2 * a_n * sizeof(float));
    // . . . . . . . . X . . .
    // . . . X . . . . . . . X
    // . . . . . . . . . . X .
    // X o X . X o X . . . . .
    // should lose the second and sixth points
    a[0] = 0.0; a[1] = 0.0;
    a[2] = 1.0; a[3] = 0.0; // disappears
    a[4] = 2.0; a[5] = 0.0;
    a[6] = 3.0; a[7] = 2.0;
    a[8] = 4.0; a[9] = 0.0;
    a[10] = 5.0; a[11] = 0.0; // disappears
    a[12] = 6.0; a[13] = 0.0;
    a[14] = 8.0; a[15] = 3.0;
    a[16] = 10.0; a[17] = 1.0;
    a[18] = 11.0; a[19] = 2.0;

    int* output_n = malloc(sizeof(int));
    float* output = malloc(2 * a_n * sizeof(float));

    float epsilon = 1.1;
    reduce_by_rdp(a_n, a, epsilon, output_n, output);

    LIBCUT_TEST_EQ(*output_n, 8);
    float correct[16] = { 0.0, 0.0,  2.0, 0.0,  3.0, 2.0,  4.0, 0.0,  6.0, 0.0,  8.0, 3.0,  10.0, 1.0,  11.0, 2.0};
    for(int i = 0; i < *output_n * 2; i++) {
        LIBCUT_TEST_EQ(output[i], correct[i]);
    }
    free(a);
    free(output_n);
    free(output);
}
LIBCUT_TEST(reduce_by_rdp_duplicates) {
    int a_n = 4;
    float* a = malloc(2 * a_n * sizeof(float));
    // . . X . .  // actually two points here
    // . . . . .
    // X . . . X
    // should lose the point at index 1 or 2)
    a[0] = 0.0; a[1] = 0.0;
    a[2] = 2.0; a[3] = 2.0; // disappears
    a[4] = 2.0; a[5] = 2.0;
    a[6] = 4.0; a[7] = 0.0;

    int* output_n = malloc(sizeof(int));
    float* output = malloc(2 * a_n * sizeof(float));

    float epsilon = 1.1;
    reduce_by_rdp(a_n, a, epsilon, output_n, output);

    LIBCUT_TEST_EQ(*output_n, 3);
    float correct[16] = { 0.0, 0.0,  2.0, 2.0,  4.0, 0.0};
    for(int i = 0; i < *output_n * 2; i++) {
        LIBCUT_TEST_EQ(output[i], correct[i]);
    }
    free(a);
    free(output_n);
    free(output);
}

LIBCUT_TEST(rdp_bench) {
    float epsilon = 0.01;
    for(int i = 1000; i <= 20000; i += 1000) {
        rdp_perf_test(i, epsilon, 100);
    }
}

LIBCUT_TEST(bench_medoid) {
    for(int i = 5; i < 15; i +=2) {
        geometric_medoid_bench(i, 1000000);
    }
}
LIBCUT_MAIN(
    geometric_medoid_small,
    median_filter_small,
    bench_medoid,
    reduce_by_rdp_small,
    reduce_by_rdp_medium,
    reduce_by_rdp_duplicates,
    rdp_bench
)

