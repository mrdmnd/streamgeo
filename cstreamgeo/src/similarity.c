#include <cstreamgeo/cstreamgeo.h>
#include <cstreamgeo/stridedmask.h>

#define PI 3.1415926535f

typedef struct {
    float warp_cost;
    strided_mask_t* path_mask;
} warp_info_t;

void warp_info_destroy(const warp_info_t* warp) {
    strided_mask_destroy(warp->path_mask);
    free((void *) warp);
}

void warp_summary_destroy(const warp_summary_t* warp) {
    free(warp->index_pairs);
    free((void *) warp);
}

warp_info_t* _full_dtw(const stream_t* a, const stream_t* b) {
    const float* a_data = a->data;
    const float* b_data = b->data;
    const size_t a_n = a->n;
    const size_t b_n = b->n;
    const size_t dp_table_height = a_n + 1;
    const size_t dp_table_width = b_n + 1;
    float* dp_table = malloc(dp_table_height * dp_table_width * sizeof(float));

    for (size_t col = 0; col < dp_table_width; col++) dp_table[col] = FLT_MAX;
    for (size_t row = 0; row < dp_table_height; row++) dp_table[row*dp_table_width] = FLT_MAX;
    dp_table[0] = 0.0;


    float diag_cost, up_cost, left_cost;
    float lat_diff, lng_diff, dt;
    size_t row, col;
    for (size_t i = 0; i < a_n; i++) {
        for (size_t j = 0; j <= b_n; j++) {
            row = i + 1;
            col = j + 1;
            lat_diff = b_data[2*j + 0] - a_data[2*i + 0];
            lng_diff = b_data[2*j + 1] - a_data[2*i + 1];
            dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
            diag_cost = dp_table[(row-1)*dp_table_width + (col-1)];
            up_cost   = dp_table[(row-1)*dp_table_width + (col  )];
            left_cost = dp_table[(row  )*dp_table_width + (col-1)];
            if (diag_cost <= up_cost && diag_cost <= left_cost) {
                dp_table[row*dp_table_width + col] = diag_cost + dt;
            }
            else if (up_cost <= left_cost) {
                dp_table[row*dp_table_width + col] = up_cost + dt;
            }
            else {
                dp_table[row*dp_table_width + col] = left_cost + dt;
            }
        }
    }
    // Trace through full window to recover path.
    size_t* reversed_index_pairs = malloc(2 * (a_n + b_n - 1) * sizeof(size_t)); // biggest possible path
    size_t u = a_n; // last index in DP table
    size_t v = b_n; // last index in DP table
    size_t path_len = 0;
    while (u > 0 && v > 0) {
        reversed_index_pairs[2*path_len + 1] = u; // not a typo, storing in reverse order for fast reversal
        reversed_index_pairs[2*path_len] = v;
        diag_cost = dp_table[(u - 1) * dp_table_width + (v - 1)];
        up_cost   = dp_table[(u - 1) * dp_table_width + (v    )];
        left_cost = dp_table[(u    ) * dp_table_width + (v - 1)];
        if (diag_cost <= up_cost && diag_cost <= left_cost) {
            u -= 1;
            v -= 1;
        } else if (up_cost <= left_cost) {
            u -= 1;
        } else {
            v -= 1;
        }
        path_len += 1;
    }
    size_t* index_pairs = malloc(2 * path_len * sizeof(size_t));
    for (size_t i = 0; i < 2*path_len; i++) {
        index_pairs[i] = reversed_index_pairs[2*path_len - 1 - i ];
    }
    free(reversed_index_pairs);

    warp_info_t* warp_info = malloc(sizeof(warp_info_t));
    warp_info->path_mask = strided_mask_from_index_pairs(index_pairs, path_len);
    warp_info->warp_cost=dp_table[dp_table_height * dp_table_width - 1];
    free(index_pairs);
    return warp_info;
}

warp_info_t* _windowed_dtw(const stream_t* a, const stream_t* b, const strided_mask_t* window) {
    const size_t a_n = a->n;
    const float* a_data = a->data;
    const size_t b_n = b->n;
    const float* b_data = b->data;

    const size_t dp_table_height = a_n + 1;
    const size_t dp_table_width = b_n + 1;

    // This is actually very expensive, and we want to be careful not to set the values in the DP table when we don't need to set them.
    float* dp_table = malloc(dp_table_height * dp_table_width * sizeof(float));
    for (size_t col = 0; col < dp_table_width; col++) dp_table[col] = FLT_MAX;
    for (size_t row = 0; row < dp_table_height; row++) dp_table[row*dp_table_width] = FLT_MAX;
    dp_table[0] = 0.0;

    warp_info_t* warp_info = malloc(sizeof(warp_info_t));


    if (window == NULL) {
        // FULL DTW

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
        // Trace through partial window to recover path
        warp_info->path_mask = NULL;
        warp_info->warp_cost=dp_table[dp_table_height * dp_table_width - 1];

    }

    // Finally, trace back through the path defined by the DP table.
    // I tested storing this path during DP table creation, but it turned out to be faster in every benchmark to
    // recreate the path ex-post-facto rather than touching extra memory.
    //            diag_cost = ( row == 1 || col == 1 || (prev_start <= col-1 && col-1 <= prev_end)) ? dp_table[m - dp_table_width - 1] : FLT_MAX;
    //            up_cost   = ( row == 1             || (prev_start <= col   && col   <= prev_end)) ? dp_table[m - dp_table_width    ] : FLT_MAX;
    //            left_cost = (             col == 1 || (     start <= col-1 && col-1 <=      end)) ? dp_table[m                  - 1] : FLT_MAX;
    free(dp_table);

    return warp_info;
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
 *     a) Downsample each stream by a factor of two
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
    warp_info_t* final_warp_info;

    if (a_n <= radius + 2 || b_n <= radius + 2) {
        final_warp_info = _dtw(a, b, NULL);
    } else {
        const stream_t* shrunk_a = _reduce_by_half(a); // Allocates memory
        const stream_t* shrunk_b = _reduce_by_half(b); // Allocates memory
        const warp_info_t* shrunk_warp_info = _fast_dtw(shrunk_a, shrunk_b, radius); // Allocates memory
        stream_destroy(shrunk_a);
        stream_destroy(shrunk_b);
        strided_mask_t* new_window = strided_mask_expand(shrunk_warp_info->path_mask, radius); // Allocates memory
        warp_info_destroy(shrunk_warp_info);
        final_warp_info = _dtw(a, b, new_window); // Allocates memory
        strided_mask_destroy(new_window);
    }
    return final_warp_info;
}




/* ---------- TOP LEVEL FUNCTIONS, EXPOSED TO API ----------- */

/**
 * A top level function to compute the optimal warp-path alignment sequence between streams a and b.
 * @param a First stream
 * @param b Second stream
 * @return A warp_summary structure containing the cost, as well as the warp path.
 */
warp_summary_t* full_align(const stream_t* a, const stream_t* b) {
    warp_summary_t* final_warp = malloc(sizeof(warp_summary_t));
    warp_info_t* warp_info = _dtw(a, b, NULL);
    final_warp->cost = warp_info->warp_cost;
    final_warp->index_pairs = strided_mask_to_index_pairs(warp_info->path_mask, &(final_warp->path_length));
    return final_warp;
}

/**
 * A top level function to compute the (approximately) optimal warp-path alignment sequence between streams a and b.
 * @param a First stream
 * @param b Second stream
 * @param radius Parameter controlling level of approximation. Larger values are slower but more comprehensive.
 * @return A warp_summary structure containing the (approximate) cost, as well as the best approximation to the warp path.
 */
warp_summary_t* fast_align(const stream_t* a, const stream_t* b, const size_t radius) {
    warp_summary_t* final_warp = malloc(sizeof(warp_summary_t));
    warp_info_t* warp_info = _fast_dtw(a, b, radius);
    final_warp->cost = warp_info->warp_cost;
    final_warp->index_pairs = strided_mask_to_index_pairs(warp_info->path_mask, &(final_warp->path_length));
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

    warp_summary_t* warp_summary = fast_align(a, b, radius);
    size_t path_length = warp_summary->path_length;
    size_t* warp_path = warp_summary->index_pairs;

    float total_weight = 0.0f;
    float total_weight_error = 0.0f;
    float unitless_cost, weight, error;
    size_t i, j;

    for (size_t n = 0; n < path_length; n++) {
        i = warp_path[2 * n];
        j = warp_path[2 * n + 1];

        lat_diff = b_data[2 * j + 0] - a_data[2 * i + 0];
        lng_diff = b_data[2 * j + 1] - a_data[2 * i + 1];
        unitless_cost = sqrtf((lng_diff * lng_diff) + (lat_diff * lat_diff)) / min_distance;
        error = (float) (1.0 - exp(-unitless_cost * unitless_cost));

        // Weight start/end less than the middle, weight sparse points less than dense.
        // This is a product of positional_weight_a * positional_weight_b * sparsity_weight_a * sparsity_weight_b
        weight = (float) (a_sparsity[i] * b_sparsity[j] * (0.1 + 0.9 * sin(PI * i / a_n)) * (0.1 + 0.9 * sin(PI * j / b_n)));
        total_weight += weight;
        total_weight_error += (error * weight);
    }

    warp_summary_destroy(warp_summary);
    free(a_sparsity);
    free(b_sparsity);

    return (float) (1.0 - total_weight_error / total_weight);
}
