#include <cstreamgeo/cstreamgeo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

/**
 * General functions for interacting with a single stream.
 * Includes resampling, filtering, sparsity, etc.
 */


stream_t* stream_create(const size_t n) {
    stream_t* stream = malloc(sizeof(stream_t));
    stream->n = n;
    stream->data = malloc(2 * n * sizeof(float));
    return stream;
}

stream_t* stream_create_from_list(const size_t n, ...) {
    va_list args;
    va_start(args, n);
    stream_t* stream = stream_create(n);
    float arg;
    for (size_t i = 0; i < 2 * n; i++) {
        arg = (float) va_arg(args, double);
        stream->data[i] = arg;
    }
    va_end(args);
    return stream;
}

void stream_destroy(const stream_t* stream) {
    free(stream->data);
    free((void*) stream);
}

void stream_printf(const stream_t* stream) {
    const size_t n = stream->n;
    const float* data = stream->data;
    printf("Stream contains %zu points: [", n);
    for (size_t i = 0; i < n; i++) {
        printf("(%f %f), ", data[2 * i], data[2 * i + 1]);
    }
    printf("]\n");
}

float stream_distance(const stream_t* stream) {
    const size_t s_n = stream->n;
    const float* data = stream->data;
    assert(s_n >= 2);
    float sum = 0.0f;
    float lat_diff;
    float lng_diff;
    for (size_t i = 0; i < s_n - 1; i++) {
        lat_diff = data[2 * i + 2] - data[2 * i + 0];
        lng_diff = data[2 * i + 3] - data[2 * i + 1];
        sum += sqrt((lng_diff * lng_diff) + (lat_diff * lat_diff));
    }
    return sum;
}

float* stream_sparsity(const stream_t* stream) {
    const size_t s_n = stream->n;
    const float* data = stream->data;
    float* sparsity = malloc(sizeof(float) * s_n);

    const float optimal_spacing = stream_distance(stream) / (s_n - 1);
    const float two_over_pi = 0.63661977236f;
    int i, j, k;
    float lat_diff, lng_diff, v;
    for (int n = 0; n < (int) s_n; n++) {
        i = (n - 1) < 0 ? 1 : n - 1;
        j = n;
        k = (n + 1) > (int) s_n - 1 ? (int) s_n - 2 : n + 1;
        lat_diff = data[2 * j] - data[2 * i];
        lng_diff = data[2 * j + 1] - data[2 * i + 1];
        float d1 = sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff));
        lat_diff = data[2 * k] - data[2 * j];
        lng_diff = data[2 * k + 1] - data[2 * j + 1];
        float d2 = sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff));
        v = (d1 + d2) / (2 * optimal_spacing);
        sparsity[j] = (float) (1.0 - two_over_pi * atan(v));
    }
    return sparsity;
}

void stream_statistics_printf(const stream_t* stream) {
    const size_t n = stream->n;
    const float distance = stream_distance(stream);
    const float* sparsity = stream_sparsity(stream);
    printf("Stream contains %zu points. Total distance is %f. Sparsity array is: [", n, distance);
    for (size_t i = 0; i < n; i++) {
        printf("%f, ", sparsity[i]);
    }
    printf("]\n");
    free((void*) sparsity);
}

float _point_line_distance(float px, float py, float sx, float sy, float ex, float ey) {
    return (sx == ex && sy == ey) ?
           (px - sx) * (px - sx) + (py - sy) * (py - sy) :
           fabsf((ex - sx) * (sy - py) - (sx - px) * (ey - sy)) / sqrtf((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy));
}

void _douglas_peucker(const stream_t* input, const size_t start, const size_t end, const float epsilon, bool* indices) {
    const float* input_data = input->data;

    float d_max = 0.0f;
    size_t index_max = start;
    float sx = input_data[2 * start];
    float sy = input_data[2 * start + 1];
    float ex = input_data[2 * end];
    float ey = input_data[2 * end + 1];
    // Identify the point with largest distance from its neighbors.
    for (size_t i = start + 1; i < end; ++i) {
        float d = _point_line_distance(input_data[2 * i], input_data[2 * i + 1], sx, sy, ex, ey);
        if (d > d_max) {
            index_max = i;
            d_max = d;
        }
    }
    // If it's at least epsilon away, we need to *keep* this point as it's "significant".
    if (d_max > epsilon) {
        // Recurse left-half.
        if (index_max - start > 1) {
            _douglas_peucker(input, start, index_max, epsilon, indices);
        }
        // Add the significant point.
        indices[index_max] = 1;
        // Recurse right-half.
        if (end - index_max > 1) {
            _douglas_peucker(input, index_max, end, epsilon, indices);
        }
    }
}


stream_t* downsample_ramer_douglas_peucker(const stream_t* input, const float epsilon) {
    const size_t input_n = input->n;
    const float* input_data = input->data;

    bool* indices = malloc(input_n * sizeof(bool));
    for (size_t i = 0; i < input_n; i++) {
        indices[i] = 0;
    }
    indices[0] = 1;
    indices[input_n - 1] = 1;

    _douglas_peucker(input, 0, input_n - 1, epsilon, indices);
    size_t n_total = 0;
    for (size_t i = 0; i < input_n; i++) {
        n_total += indices[i];
    }
    stream_t* output = stream_create(n_total);
    float* output_data = output->data;

    size_t n = 0;
    for (size_t i = 0; i < input_n; i++) {
        if (indices[i]) {
            output_data[2 * n] = input_data[2 * i];
            output_data[2 * n + 1] = input_data[2 * i + 1];
            n++;
        }
    }
    free(indices);
    return output;
}

stream_t* downsample_radial_distance(const stream_t* input, const float epsilon) {
    printf("NOT YET IMPLEMENTED\n");
    return NULL;
}

stream_t* resample_fixed_factor(const stream_t* input, const size_t n, const size_t m) {
    printf("NOT YET IMPLEMENTED\n");
    return NULL;
}
