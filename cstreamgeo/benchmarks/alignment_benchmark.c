#include <cstreamgeo/cstreamgeo.h>
#include <cstreamgeo/stridedmask.h>
#include <cstreamgeo/io.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <cstreamgeo/utilc.h>
#include "config.h" // Contains make-time generated benchmark_data_dir #define macro


void full_random_alignment_benchmark(const size_t u_n, const size_t v_n, const size_t iterations) {
    time_t seed = time(NULL);
    srand(seed);
    double time_accumulator_1 = 0;
    double time_accumulator_2 = 0;
    for (size_t j = 0; j < iterations; j++) {
        const stream_t* u = stream_create(u_n);
        float* u_data = u->data;
        const stream_t* v = stream_create(v_n);
        float* v_data = v->data;
        for (size_t i = 0; i < u_n; ++i) {
            u_data[2*i] = (float) (sqrtf(i) * 1.0 * rand() / RAND_MAX);
            u_data[2*i+1] = (float) (sqrtf(i) * 1.0 * rand() / RAND_MAX);
        }
        for (size_t i = 0; i < v_n; ++i) {
            v_data[2*i] = (float) (sqrtf(i) * 1.0 * rand() / RAND_MAX);
            v_data[2*i+1] = (float) (sqrtf(i) * 1.0 * rand() / RAND_MAX);
        }

        clock_t start, end;
        start = clock();
        warp_summary_t* warp_summary = full_warp_summary_create(u, v);
        end = clock();
        time_accumulator_1 += ((double) (end - start)) / CLOCKS_PER_SEC;
        start = clock();
        float warp_cost = full_dtw_cost(u, v);
        end = clock();
        time_accumulator_2 += ((double) (end - start)) / CLOCKS_PER_SEC;

        //assert(warp_cost == warp_summary->cost);
        warp_summary_destroy(warp_summary);
        stream_destroy(u);
        stream_destroy(v);
    }
    printf("%zu iterations of %zu by %zu full time warp took %f millis with path, %f millis without, averages=%f, %f\n",
           iterations, u_n, v_n,
           1000.0*time_accumulator_1,
           1000.0*time_accumulator_2,
           (1000.0*time_accumulator_1) / iterations,
           (1000.0*time_accumulator_2) / iterations);

}

void fast_random_alignment_benchmark(const size_t u_n, const size_t v_n, const size_t radius, const size_t iterations) {
    time_t seed = time(NULL);
    srand(seed);
    double time_accumulator = 0;
    for (size_t j = 0; j < iterations; j++) {
        const stream_t* u = stream_create(u_n);
        float* u_data = u->data;
        const stream_t* v = stream_create(v_n);
        float* v_data = v->data;
        for (size_t i = 0; i < u_n; ++i) {
            u_data[2*i] = (float) (sqrtf(i) * 1.0 * rand() / RAND_MAX);
            u_data[2*i+1] = (float) (sqrtf(i) * 1.0 * rand() / RAND_MAX);
        }
        for (size_t i = 0; i < v_n; ++i) {
            v_data[2*i] = (float) (sqrtf(i) * 1.0 * rand() / RAND_MAX);
            v_data[2*i+1] = (float) (sqrtf(i) * 1.0 * rand() / RAND_MAX);
        }

        clock_t start, end;
        start = clock();
        warp_summary_t* warp_summary = fast_warp_summary_create(u, v, radius); // Don't care about the results here; just the computation.
        end = clock();
        //printf("Warp summary: %zu points, %f cost\n", warp_summary->path_length, warp_summary->cost);
        warp_summary_destroy(warp_summary);
        time_accumulator += ((double) (end - start)) / CLOCKS_PER_SEC;

        stream_destroy(u);
        stream_destroy(v);
    }
    printf("%zu iterations of %zu by %zu fast time warp (radius %zu) with path took %f millis, average=%f\n",
           iterations, u_n, v_n, radius,
           1000.0*time_accumulator,
           (1000.0*time_accumulator) / iterations);
}

void accuracy_benchmark(const size_t u_n, const size_t v_n, const size_t radius_max) {
    time_t seed = time(NULL);
    srand(seed);
    const stream_t* u = stream_create(u_n);
    float* u_data = u->data;
    const stream_t* v = stream_create(v_n);
    float* v_data = v->data;

    clock_t start, end;
    for (size_t i = 0; i < u_n; ++i) {
        u_data[2*i] = (float) (i * 1.0 * rand() / RAND_MAX);
        u_data[2*i+1] = (float) (i * 1.0 * rand() / RAND_MAX);
    }
    for (size_t i = 0; i < v_n; ++i) {
        v_data[2*i] = (float) (i * 1.0 * rand() / RAND_MAX);
        v_data[2*i+1] = (float) (i * 1.0 * rand() / RAND_MAX);
    }
    double full_millis;
    double fast_millis;

    start = clock();
    const warp_summary_t* full_warp_summary = full_warp_summary_create(u, v);
    end = clock();
    full_millis = 1000.0*((double) (end - start)) / CLOCKS_PER_SEC;


    for (size_t radius = 0; radius <= radius_max; radius++) {
        start = clock();
        const warp_summary_t* fast_warp_summary = fast_warp_summary_create(u, v, radius);
        end = clock();
        fast_millis = 1000.0*((double) (end - start)) / CLOCKS_PER_SEC;
        printf("%zu rows\t%zu cols\t%zu radius\t%f full cost\t%f ms_full\t%f fast cost\t%f ms_fast\t%f error\n",
               u_n, v_n, radius,
               full_warp_summary->cost,
               full_millis,
               fast_warp_summary->cost,
               fast_millis,
               (fast_warp_summary->cost - full_warp_summary->cost) / full_warp_summary->cost);
        warp_summary_destroy(fast_warp_summary);
    }
    warp_summary_destroy(full_warp_summary);
    stream_destroy(u);
    stream_destroy(v);
}

void medoid_real_data_benchmark() {
    char filename[1024];
    size_t bddl = strlen(BENCHMARK_DATA_DIR);
    strcpy(filename, BENCHMARK_DATA_DIR);
    strcpy(filename+bddl, "segments/oldlahondas.json");

    const stream_collection_t* streams_from_json = read_streams_from_json(filename);
    if (!streams_from_json) {
        printf("Unable to load streams from file '%s'\n", filename);
        return;
    }

    stream_collection_printf(streams_from_json);

    int approximate = 0;
    size_t index;
    printf("Computing medoid consensus with approximate = %d \n", approximate);
    TIMEIT(
        index = medoid_consensus(streams_from_json, approximate);
    );
    printf("Index of optimal stream with approximate = %d  is %zu\n", approximate, index);

    approximate = 1;
    printf("Computing medoid consensus with approximate = %d \n", approximate);
    TIMEIT(
        index = medoid_consensus(streams_from_json, approximate);
    );
    printf("Index of optimal stream with approximate = %d  is %zu\n", approximate, index);
/*
    for (size_t index = 0; index <= streams_from_json->n; ++index) {
        printf("Downsamping a stream.\n");
        TIMEIT(
            downsample_rdp(streams_from_json->data[index], 0.0001);
        );
    }
    stream_collection_printf(streams_from_json);
*/
    stream_collection_destroy(streams_from_json);
}

int main() {
    //full_random_alignment_benchmark(4000, 4000, 20);
    //fast_random_alignment_benchmark(4000, 4000, 8, 20);
    //accuracy_benchmark(24000, 24000, 20);
    medoid_real_data_benchmark();
}

