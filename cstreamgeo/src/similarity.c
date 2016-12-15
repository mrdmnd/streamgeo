#include <cstreamgeo/cstreamgeo.h>
#include <cstreamgeo/stridedmask.h>

#define PI 3.1415926535f

/**
 * Routines for computing {fast/full} dynamic time warp distance between streams.
 */

typedef struct warp_info_s {
    float warp_cost;
    strided_mask_t* path_mask;
} warp_info_t;



/**
 * A top-level method that computes an alignment mask and DTW distance between two streams.
 * @param a_n Number of points in stream buffer `a`
 * @param a Stream buffer data for first stream
 * @param b_n Number of points in stream buffer `b`
 * @param b Stream buffer data for second stream
 * @param window Parameter containing a window mask to evaluate DTW on. Can be a "full" mask, or NULL
 * @param p Output parameter containing warp path for optimal alignment.
 * @return Cost of alignment between stream `a` and stream `b`
 */
warp_info_t* _dtw(const stream_t* a, const stream_t* b, const strided_mask_t* window) {
    const size_t a_n = a->n;
    const float* a_data = a->data;
    const size_t b_n = b->n;
    const float* b_data = b->data;

    const size_t dp_table_height = a_n + 1;
    const size_t dp_table_width = b_n + 1;
    const size_t dp_size = dp_table_height * dp_table_width;

    float* dp_table = malloc(dp_table_height * dp_table_width * sizeof(float));
    for (size_t col = 0; col < dp_table_width; col++) {
        dp_table[col] = FLT_MAX;
    }
    for (size_t row = 0; row < dp_table_height; row++) {
        dp_table[row*dp_table_width] = FLT_MAX;
    }
    dp_table[0] = 0.0;

    warp_info_t* warp_info = malloc(sizeof(warp_info_t));

    size_t m;
    float diag_cost, up_cost, left_cost;
    float lat_diff, lng_diff, dt;

    if (window == NULL) {
        // FULL DTW
        for (size_t i = 0; i < a_n; i++) {
            for (size_t j = 0; j <= b_n; j++) {
                m = (i+1)*dp_table_width + (j+1); // Unravelled index into the DP table that we're going to set.
                lat_diff = b_data[2*j + 0] - a_data[2*i + 0];
                lng_diff = b_data[2*j + 1] - a_data[2*i + 1];
                dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
                diag_cost = dp_table[m - dp_table_width - 1];
                up_cost   = dp_table[m - dp_table_width];
                left_cost = dp_table[m - 1 ];

                if (diag_cost <= up_cost && diag_cost <= left_cost) dp_table[m] = diag_cost + dt;
                else if (up_cost <= left_cost)                      dp_table[m] = up_cost + dt;
                else                                                dp_table[m] = left_cost + dt;
            }
        }

    } else {
        // WINDOWED DTW
        const size_t* start_cols = window->start_cols;
        const size_t* end_cols = window->end_cols;
        size_t start, end, prev_start, prev_end;

        for (size_t i = 0; i < a_n; i++) {
            start = start_cols[i];
            end = end_cols[i];
            if (i > 0)  {
                prev_start = start_cols[i-1];
                prev_end = end_cols[i-1];
            }
            for (size_t j = start; j <= end; j++) {
                size_t row = i+1;
                size_t col = j+1;
                m = row*dp_table_width + col; // Unravelled index into the DP table that we're going to set.
                lat_diff = b_data[2*j + 0] - a_data[2*i + 0];
                lng_diff = b_data[2*j + 1] - a_data[2*i + 1];
                dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);

                // Conditions on front are when we can actually just go look at the DP table.
                // For instance, when row == 1 in the DP table, you know that the row above you is the boundary so, so it's definitely safe to do a dp_table lookup.
                // When row and col are greater than 1
                // TODO: Might be optimizable further - could trade off more branching for one less memory access - consider that we already know the exact value when row == 1 or col == 1 in these cases.
                // i think we need to add one to col maybe in the bounds check
                diag_cost = ( row == 1 || col == 1 || (prev_start <= col-1 && col-1 <= prev_end)) ? dp_table[m - dp_table_width - 1] : FLT_MAX;
                up_cost   = ( row == 1             || (prev_start <= col   && col   <= prev_end)) ? dp_table[m - dp_table_width    ] : FLT_MAX;
                left_cost = (             col == 1 || (     start <= col-1 && col-1 <=      end)) ? dp_table[m                  - 1] : FLT_MAX;

                if (diag_cost <= up_cost && diag_cost <= left_cost) dp_table[m] = diag_cost + dt;
                else if (up_cost <= left_cost)                      dp_table[m] = up_cost + dt;
                else                                                dp_table[m] = left_cost + dt;

            }
        }
    }



    /*     1 2 3 4 5 6
     *   0 I I I I I I
     * 1 I * * * . . .
     * 1 I . * * . . .
     * 2 I . . * * . .
     * 3 I . . * * * .
     * 5 I . . . * * *
     */

    /*     1 2 3 4 5 6
     *   0 I I I I I I
     * 1 I 0 * * . . .
     * 1 I . * * . . .
     * 2 I . . * * . .
     * 3 I . . * * * .
     * 5 I . . . * * *
     */

    /*     1 2 3 4 5 6
     *   0 I I I I I I
     * 1 I 0 1 3 . . .
     * 1 I . 1 3 . . .
     * 2 I . . 2 4 . .
     * 3 I . . 2 3 5 .
     * 5 I . . . 4 3 4
     */




    // Finally, trace back through the path defined by the DP table.
    // I tested storing this path during DP table creation, but it turned out to be faster in every benchmark to
    // recreate the path ex-post-facto rather than touching extra memory.
    size_t u = a_n;
    size_t v = b_n;
    while (u > 0 && v > 0) {
        size_t idx = (u - 1) * b_n + (v - 1);
        roaring_bitmap_add(p->indices, (uint32_t) idx);
        float diag_cost = dp_table[(u - 1) * dp_table_width + (v - 1)];
        float up_cost = dp_table[(u - 1) * dp_table_width + (v)];
        float left_cost = dp_table[(u) * dp_table_width + (v - 1)];
        if (diag_cost <= up_cost && diag_cost <= left_cost) {
            u -= 1;
            v -= 1;
        } else if (up_cost <= left_cost) {
            u -= 1;
        } else {
            v -= 1;
        }
    }
    float end_cost = dp_table[dp_table_height * dp_table_width - 1];
    free(dp_table);
    return end_cost;
}


/**
 * Downsamples a stream by a factor of two, averaging consecutive points.
 * Allocates memory. Caller must clean up.
 *
 * [a, b, c, d, e, f] --> [(a+b)/2, (c+d)/2, (e+f)/2]
 * @param input_n Number of points in input buffer
 * @param input Data buffer containing original stream
 * @param output Data buffer containing down-sampled stream
 */
stream_t* _reduce_by_half(const stream_t* input) {
    const size_t input_n = input->n;
    const float* input_data = input->data;

    stream_t* shrunk_stream = stream_create(input_n / 2);
    float* shrunk_data = shrunk_stream->data;

    float lat1, lng1, lat2, lng2;

    for (size_t i = 0; i < input_n - 1; i++) {
        lat1 = input_data[2 * i + 0];
        lng1 = input_data[2 * i + 1];
        lat2 = input_data[2 * i + 2];
        lng2 = input_data[2 * i + 3];
        if (i % 2 == 0) {
            shrunk_data[i + 0] = 0.5f * (lat1 + lat2);
            shrunk_data[i + 1] = 0.5f * (lng1 + lng2);
        }
    }
    return shrunk_stream;
}

/**
 * A method that computes the dynamic time warp alignment between two streams `a` and `b` using the
 * approximate algorithm fast_dtw. At a high level, this algorithm works by recursively
 *   - Base case: small stream size; compute explicit DTW
 *   - Recursive case:
 *     a) Downsamplie each stream by a factor of two
 *     b) Call fast_dtw recursively on the downsampled streams to get a shrunk alignment path
 *     c) Expand the shrunk alignment path by some radius into a new search window
 *        (this reprojects the alignment path back into the resolution of the original streams)
 *     d) Compute the optimal alignment with traditional DTW over the new search window
 *
 * @param a_n Number of points in the first stream
 * @param a Stream buffer for the first stream
 * @param b_n Number of points in the second stream
 * @param b Stream buffer for the second stream
 * @param radius Amount to extrude shrunk path by: set to one by default. Larger values are more exhaustive search.
 * @param path Output parameter containing optimal alignment path
 * @return Alignment cost of optimal alignment path
 */
warp_info_t* _fast_dtw(const stream_t* a, const stream_t* b, const size_t radius) {
    const size_t a_n = a->n;
    const size_t b_n = b->n;
    const size_t min_size = radius + 2;

    if (a_n <= min_size || b_n <= min_size) {
        // Sets path output variable through side-effects.
        return _dtw(a, b, NULL, path);
    } else {
        stream_t* shrunk_a = _reduce_by_half(a);
        stream_t* shrunk_b = _reduce_by_half(b);

        // Worse case path is all the way down, then all the way right (minus one because this double-counts corner).
        roaring_mask_t* shrunk_path = roaring_mask_create(a_n / 2, b_n / 2, a_n / 2 + b_n / 2 - 1);

        // Calculate a shrunk path to feed into the expand_window fn
        _fast_dtw(shrunk_a, shrunk_b, radius, shrunk_path);
        stream_destroy(shrunk_a);
        stream_destroy(shrunk_b);

        // It takes some cleverness to figure out the maximum number of cells that we'll see
        // The worst case is when a_n = b_n, and the shrunk_path passes exactly on the diagonal
        // see page six of http://cs.fit.edu/~pkc/papers/tdm04.pdf for more
        roaring_mask_t* new_window = roaring_mask_create(a_n, b_n, (4 * radius + 3) * MAX(a_n, b_n));
        _expand_window(shrunk_path, radius, new_window);
        roaring_mask_destroy(shrunk_path);
        float result = _dtw(a, b, new_window, path);
        roaring_mask_destroy(new_window);
        return result;
    }
}





/* ---------- TOP LEVEL FUNCTIONS, EXPOSED TO API ----------- */

/**
 * A top level function to compute the optimal warp-path alignment sequence between streams a and b.
 * @param a First stream
 * @param b Second stream
 * @param cost Cost of optimal alignment (output param)
 * @param path_length Number of cells in the warp path (output param)
 * @return Array of the form (i0, j0, i1, j1, i2, j2, ...) with 2*path_length entries.
 */
size_t* full_align(const stream_t* a, const stream_t* b, float* cost, size_t* path_length) {
    roaring_mask_t* mask = roaring_mask_create(a->n, b->n, a->n + b->n - 1);
    *cost = _dtw(a, b, NULL, mask);
    size_t* warp_path = roaring_mask_to_index_pairs(mask, path_length);
    return warp_path;
}

/**
 * A top level function to compute the (approximately) optimal warp-path alignment sequence between streams a and b.
 * @param a First stream
 * @param b Second stream
 * @param radius Parameter controlling level of approximation. Larger values are slower but more comprehensive.
 * @param cost Cost of optimal alignment (output param)
 * @param path_length Number of cells in the warp path (output param)
 * @return Array of the form (i0, j0, i1, j1, i2, j2, ...) with 2*path_length entries.
 */
size_t* fast_align(const stream_t* a, const stream_t* b, const size_t radius, float* cost, size_t* path_length) {
    roaring_mask_t* mask = roaring_mask_create(a->n, b->n, a->n + b->n - 1);
    *cost = _fast_dtw(a, b, radius, mask);
    size_t* warp_path = roaring_mask_to_index_pairs(mask, path_length);
    return warp_path;
}

/**
 * A top level, short-circuiting function that establishes a distance metric on stream data.
 * Returns values in the range [0.0, 1.0] - 0.0 means totally dissimilar, 1.0 means identical.
 * Algorithm for similarity checks easiest "fail" cases first, for efficiency:
 *   a) if streams have very different distance ratios (2.5x), return 0.0
 *   b) if stream start/mid/endpoints are further apart than 30% of min distance, return 0.0
 *   c) otherwise, compute sparsity of each stream to get a weight for each point, and run the _iterative_similarity() method above
 * @param a_n Number of points in the first stream
 * @param a Stream buffer for the first stream
 * @param b_n Number of points in the second stream
 * @param b Stream buffer for the second stream
 * @param radius Extrusion radius for alignment computation
 * @return Computed similarity between two streams, between zero and one.
 */
float redmond_similarity(const stream_t* a, const stream_t* b, const size_t radius) {
    const size_t a_n = a->n;
    const float* a_data = a->data;
    const size_t b_n = b->n;
    const float* b_data = b->data;

    // Stream is improper
    if (a_n < 2 || b_n < 2) {
        return 0.0;
    }
    // Horribly mismatched distance ratio
    const float a_stream_distance = stream_distance(a);
    const float b_stream_distance = stream_distance(b);
    const float ratio = a_stream_distance / b_stream_distance;
    if (ratio < 0.4 || ratio > 2.5) {
        return 0.0;
    }
    // If start/mid/endpoints are further apart than 30% of min distance, return zero
    const float min_distance = 0.3f * MIN(a_stream_distance, b_stream_distance);
    // Start point
    float lat_diff = b_data[2 * (0) + 0] - a_data[2 * (0) + 0];
    float lng_diff = b_data[2 * (0) + 1] - a_data[2 * (0) + 1];
    if (sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff)) > min_distance) return 0.0;
    // Midpoint
    lat_diff = b_data[2 * (b_n / 2) + 0] - a_data[2 * (a_n / 2) + 0];
    lng_diff = b_data[2 * (b_n / 2) + 1] - a_data[2 * (a_n / 2) + 1];
    if (sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff)) > min_distance) return 0.0;
    // Endpoint
    lat_diff = b_data[2 * (b_n - 1) + 0] - a_data[2 * (a_n - 1) + 0];
    lng_diff = b_data[2 * (b_n - 1) + 1] - a_data[2 * (a_n - 1) + 1];
    if (sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff)) > min_distance) return 0.0;

    float* a_sparsity = stream_sparsity(a);
    float* b_sparsity = stream_sparsity(b);

    float* cost = malloc(sizeof(float));
    *cost = 0.0f;
    size_t* path_length = malloc(sizeof(size_t));
    *path_length = 0;

    size_t* warp_path = fast_align(a, b, radius, cost, path_length);

    float total_weight = 0.0f;
    float total_weight_error = 0.0f;
    float unitless_cost, weight, error;
    size_t i, j;

    for (size_t n = 0; n < *path_length; n++) {
        i = warp_path[2 * n];
        j = warp_path[2 * n + 1];

        lat_diff = b_data[2 * j + 0] - a_data[2 * i + 0];
        lng_diff = b_data[2 * j + 1] - a_data[2 * i + 1];
        unitless_cost = sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff)) / min_distance;
        error = (float) (1.0 - exp(-unitless_cost * unitless_cost));

        // Weight start/end less than the middle, weight sparse points less than dense.
        // This is a product of positional_weight_a * positional_weight_b * sparsity_weight_a * sparsity_weight_b
        weight = (float) (a_sparsity[i] * b_sparsity[j] * (0.1 + 0.9 * sin(PI * i / a_n)) *
                          (0.1 + 0.9 * sin(PI * j / b_n)));
        total_weight += weight;
        total_weight_error += (error * weight);
    }

    free(cost);
    free(path_length);
    free(warp_path);
    free(a_sparsity);
    free(b_sparsity);

    return (float) (1.0 - total_weight_error / total_weight);
}
