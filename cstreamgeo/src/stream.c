#include <cstreamgeo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>

/**
 * General functions for interacting with a single stream.
 * Includes resampling, filtering, sparsity, etc.
 */


stream_t* stream_create(const size_t n) {
    stream_t* stream = malloc(sizeof(stream_t));
    stream->n = n;
    stream->lats = malloc(n * sizeof(int32_t));
    stream->lngs = malloc(n * sizeof(int32_t));
    return stream;
}

void stream_destroy(const stream_t* stream) {
    free(stream->lats);
    free(stream->lngs);
    free((void*) stream);
}

stream_t* stream_create_from_list(const size_t n, ...) {
    va_list args;
    va_start(args, n);
    stream_t* stream = stream_create(n);
    for (size_t i = 0; i < 2 * n; i++) {
        stream->lats[i] = (int32_t)(1000000*va_arg(args, double));
    }
    va_end(args);
    return stream;
}



void stream_printf(const stream_t* stream) {
    const size_t n = stream->n;
    const int32_t* lats = stream->lats;
    const int32_t* lngs = stream->lats;
    printf("Stream contains %zu points: [", n);
    for (size_t i = 0; i < n; i++) {
        printf("[%f, %f], ", lats[i]/1000000.0, lngs[i]/1000000.0);
    }
    printf("]\n");
}

void stream_geojson_printf(const stream_t* stream) {
    const size_t n = stream->n;
    const int32_t* lats = stream->lats;
    const int32_t* lngs = stream->lngs;
    printf("{ \"type\": \"Feature\", \"properties\": {}, \"geometry\": { \"type\": \"LineString\", \"coordinates\": [");
    for (size_t i = 0; i < n-1; i++) {
        printf("[%f, %f], ", lngs[i]/1000000.0, lats[i]/1000000.0);
    }
    printf("[%f, %f]]", lngs[n-1]/1000000.0, lats[n-1]/1000000.0);
    printf("}}\n");
}

float stream_distance(const stream_t* stream) {
    const size_t n = stream->n;
    const int32_t* lats = stream->lats;
    const int32_t* lngs = stream->lngs;
    assert(n >= 2);
    float sum = 0.0f;
    int32_t lat_diff;
    int32_t lng_diff;
    for (size_t i = 0; i < n - 1; i++) {
        lat_diff = lats[i + 1] - lats[i];
        lng_diff = lngs[i + 1] - lngs[i];
        sum += sqrt((lng_diff * lng_diff) + (lat_diff * lat_diff));
    }
    return sum;
}

float* stream_sparsity_create(const stream_t *stream) {
    const size_t s_n = stream->n;
    const int32_t* lats = stream->lats;
    const int32_t* lngs = stream->lngs;
    float* sparsity = malloc(sizeof(float) * s_n);

    const float optimal_spacing = stream_distance(stream) / (s_n - 1);
    const float two_over_pi = 0.63661977236f;
    int i, j, k;
    float lat_diff, lng_diff, v;
    for (int n = 0; n < (int) s_n; n++) {
        i = (n - 1) < 0 ? 1 : n - 1;
        j = n;
        k = (n + 1) > (int) s_n - 1 ? (int) s_n - 2 : n + 1;
        lat_diff = lats[j] - lats[i];
        lng_diff = lngs[j] - lngs[i];
        float d1 = sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff));
        lat_diff = lats[k] - lats[j];
        lng_diff = lngs[k] - lngs[j];
        float d2 = sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff));
        v = (d1 + d2) / (2 * optimal_spacing);
        sparsity[j] = (float) (1.0 - two_over_pi * atan(v));
    }
    return sparsity;
}

void stream_statistics_printf(const stream_t* stream) {
    const size_t n = stream->n;
    const float distance = stream_distance(stream);
    const float* sparsity = stream_sparsity_create(stream);
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

void _douglas_peucker(const stream_t* stream, const size_t start, const size_t end, const float epsilon, bool* indices) {
    const int32_t* lats = stream->lats;
    const int32_t* lngs = stream->lngs;
    float d_max = 0.0f;
    size_t index_max = start;
    float sx = lats[start];
    float sy = lngs[start];
    float ex = lats[end];
    float ey = lngs[end];
    // Identify the point with largest distance from its neighbors.
    for (size_t i = start + 1; i < end; ++i) {
        float d = _point_line_distance(lats[i], lngs[i], sx, sy, ex, ey);
        if (d > d_max) {
            index_max = i;
            d_max = d;
        }
    }
    // If it's at least epsilon away, we need to *keep* this point as it's "significant".
    if (d_max > epsilon) {
        // Recurse left-half.
        if (index_max - start > 1) {
            _douglas_peucker(stream, start, index_max, epsilon, indices);
        }
        // Add the significant point.
        indices[index_max] = 1;
        // Recurse right-half.
        if (end - index_max > 1) {
            _douglas_peucker(stream, index_max, end, epsilon, indices);
        }
    }
}


void downsample_rdp(stream_t* stream, const float epsilon) {
    size_t input_n = stream->n;
    int32_t* input_lats = stream->lats;
    int32_t* input_lngs = stream->lngs;

    bool* indices = calloc(input_n, sizeof(bool));
    indices[0] = 1;
    indices[input_n - 1] = 1;

    _douglas_peucker(stream, 0, input_n - 1, epsilon, indices);
    // Indices is an array 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, ... 1

    size_t n = 0;
    for (size_t i = 0; i < input_n; i++) {
        if (indices[i]) {
            input_lats[n] = input_lats[i];
            input_lngs[n] = input_lngs[i];
            n++;
        }
    }
    stream->n = n;
    stream->lats = realloc(input_lats, n * sizeof(int32_t));
    stream->lngs = realloc(input_lngs, n * sizeof(int32_t));
    free(indices);
}

/*
stream_t* downsample_radial_distance(const stream_t* input, const float epsilon) {
    printf("NOT YET IMPLEMENTED\n");
    return NULL;
}

stream_t* resample_fixed_factor(const stream_t* input, const size_t n, const size_t m) {
    printf("NOT YET IMPLEMENTED\n");
    return NULL;
}
*/
