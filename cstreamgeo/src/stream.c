#include <cstreamgeo/cstreamgeo.h>

/**
 * General functions for interacting with a single stream.
 * Includes resampling, filtering, sparsity, etc.
 */


stream_t* stream_create(const size_t n) {
    stream_t *stream = malloc(sizeof(stream_t));
    stream->n = n;
    stream->data = malloc(2 * n * sizeof(float));
    return stream;
}

void stream_destroy(const stream_t* stream) {
    free(stream->data);
    free(stream);
}


/**
 * Returns the length of a stream in degrees (note: not meters)
 * @param s_n Number of points in the stream
 * @param s Stream data buffer
 * @return Length of the stream in degrees
 */
float stream_distance(const stream_t* stream) {
    const size_t s_n = stream->n;
    const float* data = stream->data;
    assert(s_n >= 2);
    float sum = 0.0f;
    float lat_diff;
    float lng_diff;
    for(int i = 0; i < s_n-1; i++) {
        lat_diff = data[2*i+2] - data[2*i+0];
        lng_diff = data[2*i+3] - data[2*i+1];
        sum += sqrt((lng_diff * lng_diff) + (lat_diff * lat_diff));
    }
    return sum;
}


/**
 * For each point in the stream buffer, compute the summed distance from that point to both neighbors --
 * the first and last point in the buffer count their neighbor twice.
 * Take the summed distance and normalize by 2 * optimal_spacing - the distance that each point would be from each
 * other point if the stream were perfectly spaced.
 * This procedure assigns a value [0, \infty) to each point.
 * Convert this domain into the range [0, 1] by passing the values x through 1- 2*atan(x)/pi.
 * @param s_n Number of points in the stream
 * @param s Stream buffer data
 * @return Outputs sparsity data. Assumed to have s_n points. Allocates memory. Caller must clean up.
 */
float* stream_sparsity(const stream_t* stream) {
    const size_t s_n = stream->n;
    const float* data = stream->data;
    float* sparsity = malloc(sizeof(float) * s_n);

    const float optimal_spacing = stream_distance(stream) / (s_n-1);
    const float two_over_pi = 0.63661977236f;
    size_t i, j, k;
    float lat_diff, lng_diff, v;
    for(size_t n = 0; n < s_n; n++) {
        i = (n - 1) < 0 ? 1 : n - 1;
        j = n;
        k = (n + 1) > s_n - 1 ? s_n - 2 : n + 1;
        lat_diff = data[2*j] - data[2*i];
        lng_diff = data[2*j + 1] - data[2*i + 1];
        float d1 = sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff));
        lat_diff = data[2*k] - data[2*j];
        lng_diff = data[2*k + 1] - data[2*j + 1];
        float d2 = sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff));
        v = (d1 + d2) / (2 * optimal_spacing);
        sparsity[j] = (float) (1.0 - two_over_pi * atan(v));
    }
    return sparsity;
}

/**
 * Computes the distance from a point to a line (defined by a start and end point), in two dimensions.
 * @param px Test point x
 * @param py Test point y
 * @param sx Start point x
 * @param sy Start point y
 * @param ex End point x
 * @param ey End point y
 * @return Distance between point p and line (s, e)
 */
float _point_line_distance(float px, float py, float sx, float sy, float ex, float ey) {
    return (sx == ex && sy == ey) ?
           (px-sx)*(px-sx) + (py-sy)*(py-sy) :
           fabsf((ex-sx)*(sy-py) - (sx-px)*(ey-sy)) / sqrtf((ex-sx)*(ex-sx) + (ey-sy)*(ey-sy));
}

/**
 * Performs polyline simplification and identifies the indices to keep from a data buffer of points.
 * @param input Stream to simplify
 * @param start Start index to search from
 * @param end End index to stop searching at
 * @param epsilon Threshold for keeping points (keep if they exceed epsilon distance from line segment bound)
 * @param indices Output object containing indices of keep points.
 */
void _douglas_peucker(const stream_t* input, const size_t start, const size_t end, const float epsilon, roaring_bitmap_t* indices) {
    const float* input_data = input->data;

    float d_max = 0.0f; size_t index_max = start;
    float sx = input_data[2*start]; float sy = input_data[2*start+1];
    float ex = input_data[2*end];   float ey = input_data[2*end+1];
    // Identify the point with largest distance from its neighbors.
    for (size_t i = start+1; i < end; ++i) {
        float d = _point_line_distance(input_data[2*i], input_data[2*i+1], sx, sy, ex, ey);
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
        roaring_bitmap_add(indices, (uint32_t)index_max);
        // Recurse right-half.
        if (end - index_max > 1) {
            _douglas_peucker(input, index_max, end, epsilon, indices);
        }
    }
}

/**
 * Wrapper around _douglas_peucker to be called externally
 * @param input Input stream
 * @param epsilon Threshold for keeping points (keep if they exceed epsilon distance)
 * @param output Output stream (pre-allocated)
 */
void downsample_ramer_douglas_peucker(const stream_t* input, const float epsilon, stream_t* output) {
    const size_t input_n = input->n;
    const float* input_data = input->data;

    float* output_data = output->data;

    roaring_bitmap_t* indices = roaring_bitmap_create();
    roaring_bitmap_add(indices, 0);
    roaring_bitmap_add(indices, (uint32_t)input_n-1);
    _douglas_peucker(input, 0, input_n-1, epsilon, indices);
    size_t n = 0;
    for (size_t i = 0; i < input_n; i++) {
        if (roaring_bitmap_contains(indices, (uint32_t)i)) {
            output_data[2*n] = input_data[2*i];
            output_data[2*n+1] = input_data[2*i+1];
            n++;
        }
    }
    output->n = n;
    roaring_bitmap_free(indices);
}
