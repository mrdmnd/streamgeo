#include <cstreamgeo.h>
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


void stream_init(stream_t* stream, const size_t n, const stream_type_t stream_type) {
    switch(stream_type) {
        case POSITION:
            stream->stream_contents.position_stream.points = malloc(n * sizeof(point_t));
            stream->stream_contents.position_stream.n = n;
            break;
        case TIME:
            stream->stream_contents.timestamp_stream.timestamps = malloc(n * sizeof(timestamp_t));
            stream->stream_contents.timestamp_stream.n = n;
            break;
        case ELEVATION:
            stream->stream_contents.elevation_stream.elevations = malloc(n * sizeof(elevation_t));
            stream->stream_contents.elevation_stream.n = n;
            break;
    }
}

void stream_init_from_list(stream_t* stream, const size_t n, const stream_type_t stream_type, ...) {
    va_list args;
    va_start(args, n);
    // Init blank storage
    stream_init(stream, n, stream_type);
    // Fill storage with data
    switch(stream_type) {
        case POSITION:
            for (size_t i = 0; i < n; i++) {
                int32_t lat = (int32_t)(va_arg(args, double));
                int32_t lng = (int32_t)(va_arg(args, double));
                point_t point = (point_t) {lat, lng};
                stream->stream_contents.position_stream.points[i] = point;
            }
            break;
        case TIME:
            for (size_t i = 0; i < n; i++) {
                timestamp_t time = va_arg(args, int);
                stream->stream_contents.timestamp_stream.timestamps[i] = time;
            }
            break;
        case ELEVATION:
            for (size_t i = 0; i < n; i++) {
                elevation_t elevation = va_arg(args, int);
                stream->stream_contents.elevation_stream.elevations[i] = elevation;
            }
            break;
    }
    va_end(args);
}

void stream_release(const stream_t* stream) {
    switch(stream->stream_type) {
        case POSITION:
            free(stream->stream_contents.position_stream.points);
            break;
        case TIME:
            free(stream->stream_contents.timestamp_stream.timestamps);
            break;
        case ELEVATION:
            free(stream->stream_contents.elevation_stream.elevations);
            break;
    }

}

void stream_printf(const stream_t* stream) {
    position_stream_t a = stream->stream_contents.position_stream;
    timestamp_stream_t b = stream->stream_contents.timestamp_stream;
    elevation_stream_t c = stream->stream_contents.elevation_stream;
    switch(stream->stream_type) {
        case POSITION:
            printf("Stream contains %zu position points: [", a.n);
            for (size_t i = 0; i < a.n; i++) {
                point_t p = a.points[i];
                printf("[%i, %i], ", p.lat, p.lng);
            }
            printf("]\n");
            break;
        case TIME:
            printf("Stream contains %zu timestamp points: [", b.n);
            for (size_t i = 0; i < b.n; i++) {
                timestamp_t timestamp = b.timestamps[i];
                printf("%zu, ", (size_t) timestamp);
            }
            printf("]\n");
            break;
        case ELEVATION:
            printf("Stream contains %zu elevation points: [", c.n);
            for (size_t i = 0; i < c.n; i++) {
                elevation_t elevation = c.elevations[i];
                printf("%zu, ", (size_t) elevation);
            }
            printf("]\n");
            break;
    }
}

void stream_geojson_printf(const position_stream_t* stream) {
    point_t p;
    printf("{ \"type\": \"Feature\", \"properties\": {}, \"geometry\": { \"type\": \"LineString\", \"coordinates\": [");
    for (size_t i = 0; i < stream->n-1; i++) {
        p = stream->points[i];
        printf("[%i, %i], ", p.lng, p.lat);
    }
    p = stream->points[stream->n - 1];
    printf("[%i, %i]]", p.lng, p.lat);
    printf("}}\n");
}

double stream_distance(const position_stream_t* stream) {
    const size_t n = stream->n;
    const point_t* points = stream->points;
    assert(n >= 2);
    double sum = 0.0f;
    point_t current;
    point_t next;
    point_t diff;
    for (size_t i = 0; i < n - 1; i++) {
        current = points[i];
        next = points[i+1];
        diff = (point_t){next.lat - current.lat, next.lng - current.lng};
        sum += sqrtl(diff.lat * diff.lat + diff.lng * diff.lng);
    }
    return sum;
}

void stream_sparsity_init(const position_stream_t* stream, float* sparsity_out) {
    const size_t s_n = stream->n;
    const point_t* points = stream->points;
    const float optimal_spacing = (float) (stream_distance(stream) / (s_n - 1));
    const float two_over_pi = 0.63661977236f;
    int i, j, k;
    float v;
    point_t diff;
    for (int n = 0; n < (int) s_n; n++) {
        i = (n - 1) < 0 ? 1 : n - 1;
        j = n;
        k = (n + 1) > (int) s_n - 1 ? (int) s_n - 2 : n + 1;
        point_t p_i = points[i];
        point_t p_j = points[j];
        point_t p_k = points[k];
        diff = (point_t){p_j.lat - p_i.lat, p_j.lng - p_i.lng};
        float d1 = sqrtf(diff.lng * diff.lng + diff.lat * diff.lat);
        diff = (point_t){p_k.lat - p_j.lat, p_k.lng - p_j.lng};
        float d2 = sqrtf(diff.lng * diff.lng + diff.lat * diff.lat);
        v = (d1 + d2) / (2 * optimal_spacing);
        sparsity_out[j] = (float) (1.0 - two_over_pi * atan(v));
    }
}

void stream_statistics_printf(const position_stream_t* stream) {
    const size_t n = stream->n;
    const double distance = stream_distance(stream);
    float* sparsity = malloc(n * sizeof(float));
    stream_sparsity_init(stream, sparsity);
    printf("Stream contains %zu points. Total distance is %f. Sparsity array is: [", n, distance);
    for (size_t i = 0; i < n; i++) {
        printf("%f, ", sparsity[i]);
    }
    printf("]\n");
    free((void*) sparsity);
}

float _point_line_distance(point_t p, point_t s, point_t e) {
    return (s.lat == e.lat && s.lng == e.lng) ?
           (p.lat - s.lat) * (p.lat - s.lat) + (p.lng - s.lng) * (p.lng - s.lng) :
           abs((e.lat - s.lat) * (s.lng - p.lng) - (s.lat - p.lat) * (e.lng - s.lng)) / sqrtf((e.lat - s.lat) * (e.lat - s.lat) + (e.lng - s.lng) * (e.lng - s.lng));
}

void _douglas_peucker(const position_stream_t* stream, const size_t start, const size_t end, const float epsilon, bool* indices) {
    const point_t* points = stream->points;
    float d_max = 0.0f;
    size_t index_max = start;
    point_t s = points[start];
    point_t e = points[end];
    // Identify the point with largest distance from its neighbors.
    point_t p;
    for (size_t i = start + 1; i < end; ++i) {
        p = points[i];
        float d = _point_line_distance(p, s, e);
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


void downsample_rdp(position_stream_t* stream, const float epsilon) {
    size_t input_n = stream->n;
    point_t* input_points = stream->points;

    bool* indices = calloc(input_n, sizeof(bool));
    indices[0] = 1;
    indices[input_n - 1] = 1;

    _douglas_peucker(stream, 0, input_n - 1, epsilon, indices);
    // Indices is an array 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, ... 1

    size_t n = 0;
    for (size_t i = 0; i < input_n; i++) {
        if (indices[i]) {
            input_points[n] = input_points[i];
            n++;
        }
    }
    stream->n = n;
    stream->points = realloc(input_points, n * sizeof(point_t));
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
