#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cstreamgeo/cstreamgeo.h>

void full_random_alignment_benchmark(const size_t u_n, const size_t v_n, const size_t iterations) {
    double time_accumulator = 0;
    for (size_t j = 0; j < iterations; j++) {
        time_t seed = time(NULL);
        printf("Seed is %zu\n", seed);
        srand(seed);

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
        warp_summary_t* warp_summary = full_align(u, v);
        end = clock();
        free(warp_summary->index_pairs); // BROKEN
        free((void*) warp_summary);
        time_accumulator += ((double) (end - start)) / CLOCKS_PER_SEC;

        stream_destroy(u);
        stream_destroy(v);
    }
    printf("%zu iterations of %zu by %zu full time warp with path took %f millis, average=%f\n",
           iterations, u_n, v_n,
           1000.0*time_accumulator,
           (1000.0*time_accumulator) / iterations);
}

void fast_random_alignment_benchmark(const size_t u_n, const size_t v_n, const size_t radius, const size_t iterations) {
    double time_accumulator = 0;
    for (size_t j = 0; j < iterations; j++) {
        time_t seed = time(NULL);
        printf("Seed is %zu\n", seed);
        srand(seed);

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
        warp_summary_t* warp_summary = fast_align(u, v, radius); // Don't care about the results here; just the computation.
        end = clock();
        free(warp_summary->index_pairs);
        free((void*) warp_summary);
        time_accumulator += ((double) (end - start)) / CLOCKS_PER_SEC;

        stream_destroy(u);
        stream_destroy(v);
    }
    printf("%zu iterations of %zu by %zu fast time warp with path took %f millis, average=%f\n",
           iterations, u_n, v_n,
           1000.0*time_accumulator,
           (1000.0*time_accumulator) / iterations);
}

void accuracy_benchmark(const size_t u_n, const size_t v_n, const size_t radius_max) {
    srand(time(NULL));
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
    const warp_summary_t* full_warp_summary = full_align(u, v);
    end = clock();
    full_millis = 1000.0*((double) (end - start)) / CLOCKS_PER_SEC;


    for (size_t radius = 0; radius < radius_max; radius++) {
        start = clock();
        const warp_summary_t* fast_warp_summary = fast_align(u, v, radius);
        end = clock();
        fast_millis = 1000.0*((double) (end - start)) / CLOCKS_PER_SEC;
        printf("%zu rows\t%zu cols\t%zu radius\t%f full cost\t%f ms_full\t%f fast cost\t%f ms_fast\t%f error\n",
               u_n, v_n, radius,
               full_warp_summary->cost,
               full_millis,
               fast_warp_summary->cost,
               fast_millis,
               (fast_warp_summary->cost - full_warp_summary->cost) / full_warp_summary->cost);
        free(fast_warp_summary->index_pairs);
        free((void*) fast_warp_summary);
    }
    free(full_warp_summary->index_pairs);
    free((void*) full_warp_summary);
    stream_destroy(u);
    stream_destroy(v);
}

int main() {
    //full_random_alignment_benchmark(4000, 4000, 30);
    //fast_random_alignment_benchmark(10, 11, 1, 1);
    accuracy_benchmark(400, 415, 10);
}

