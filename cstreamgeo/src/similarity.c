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


// TERRIBLE SCARY FLOATING POINT HACKERY AHEAD
// We store *step-direction* in the low bits of the mantissa - this is a space/accuracy trade off.
// 11 means "diag", 10 means "up", 01 means "left"
// and-ing with ~0x0003 (00 mask) lets us recover the true cost from the dp table
typedef union {
    float f;
    struct {
        unsigned int mantissa : 23;
        unsigned int exponent : 8;
        unsigned int sign : 1;
    } parts;
} unpacked_float_t;

warp_info_t* _full_dtw(const stream_t* a, const stream_t* b) {
    const float* a_data = a->data;
    const float* b_data = b->data;
    const size_t a_n = a->n;
    const size_t b_n = b->n;
    unpacked_float_t* dp_table = malloc(a_n * b_n * sizeof(unpacked_float_t));

    dp_table[0].f = 0.0;


    unpacked_float_t diag_cost, up_cost, left_cost;
    float lat_diff, lng_diff, dt;


    for (size_t row = 0; row < a_n; row++) {
        for (size_t col = 0; col < b_n; col++) {
            lat_diff = b_data[2*col + 0] - a_data[2*row + 0];
            lng_diff = b_data[2*col + 1] - a_data[2*row + 1];
            dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
            // TODO: benchmark against frexp function calls rather than using the union above.
            diag_cost.f = (row == 0 || col == 0) ? FLT_MAX : dp_table[(row-1)*b_n + (col-1)].f;
            diag_cost.parts.mantissa &= ~0x0003;
            up_cost.f  =  (row == 0            ) ? FLT_MAX : dp_table[(row-1)*b_n + (col  )].f;
            up_cost.parts.mantissa &= ~0x0003;
            left_cost.f = (            col == 0) ? FLT_MAX : dp_table[(row  )*b_n + (col-1)].f;
            left_cost.parts.mantissa &= ~0x0003;

            // Special note: the (0,0) case is weird on boundary conditions -- all its costs are FLT_MAX, but we want it to be zero
            // so the diag_cost compares favorable here and we do the right thing
            if (row == 0 && col == 0) {
                dp_table[0].f = 0 + dt;
                dp_table[0].parts.mantissa |= 0x0003;
            }
            else if (diag_cost.f <= up_cost.f && diag_cost.f <= left_cost.f) {
                dp_table[row*b_n + col].f = diag_cost.f + dt;
                dp_table[row*b_n + col].parts.mantissa |= 0x0003; // Set the direction bits for this cell.
            }
            else if (up_cost.f <= left_cost.f) {
                dp_table[row*b_n + col].f = up_cost.f + dt;
                dp_table[row*b_n + col].parts.mantissa |= 0x0002;
            }
            else {
                dp_table[row*b_n + col].f = left_cost.f + dt;
                dp_table[row*b_n + col].parts.mantissa |= 0x0001;
            }
        }
    }

    // DEBUG: print the dp_table
    /*
    for (size_t row = 0; row < a_n; row++) {
        for (size_t col = 0; col < b_n; col++) {
            unpacked_float_t value = dp_table[row*b_n + col];
            unsigned int direction = value.parts.mantissa & 0x0003;
            if (direction==3) {
                printf("↖");
            } else if (direction==2) {
                printf("↥");
            } else {
                printf("↤");
            }
        }
        printf("\n");
    }*/

    size_t u = a_n - 1;
    size_t v = b_n - 1;
    strided_mask_t* mask = strided_mask_create(a_n, b_n); // sometimes this line seems to create masks with not enough columns?
    size_t* start_cols = mask->start_cols;
    size_t* end_cols = mask->end_cols;

    size_t path_len = 1;
    start_cols[0] = 0;
    end_cols[u] = v;

    while(u > 0 && v > 0) {
        path_len++;
        size_t index = u*b_n + v;
        unpacked_float_t cost = dp_table[index];
        unsigned int result = cost.parts.mantissa & 0x0003;
        if (result == 0x0003) {
            // This was a diagonal step: we know that the current column (v) is the "start" column for this row (u),
            // and that the column one to our left (v-1) is the "end" column for the row above us (u-1).
            start_cols[u] = v;
            end_cols[u-1] = v-1;
            u -= 1;
            v -= 1;
        } else if (result == 0x0002) {
            // This was a vertical step: we know that the current column (v) is the "start" column for this row (u),
            // and that the current column (v) is also the "end" column for the row above this row (u-1)
            start_cols[u] = v;
            end_cols[u-1] = v;
            u -= 1;
        } else {
            // This was a horizontal step. We don't learn anything about the start or end columns.
            v -= 1;
        }
    }
    // TODO: This is super janky; figure out how to do it better.
    // Problem: if you stop iterating when u > 0 and v > 0, then any time the paths ends up in a sequence of vertical steps, you don't capture them, because v is already zero
    if (v == 0) {
        while (u > 0) {
            start_cols[u] = 0;
            end_cols[u] = 0;
            u -= 1;
        }
        end_cols[0] = 0;
    }



    warp_info_t* warp_info = malloc(sizeof(warp_info_t));
    warp_info->path_mask = mask;
    unpacked_float_t final_cost = dp_table[a_n*b_n - 1];
    final_cost.parts.mantissa &= ~0x0003;
    warp_info->warp_cost=final_cost.f;
    free(dp_table);
    return warp_info;
}


warp_info_t* _windowed_dtw(const stream_t* a, const stream_t* b, const strided_mask_t* window) {
    strided_mask_printf(window);
    const float* a_data = a->data;
    const float* b_data = b->data;
    const size_t a_n = a->n;
    const size_t b_n = b->n;
    const size_t dp_table_height = a_n + 1;
    const size_t dp_table_width = b_n + 1;
    const size_t* initial_start_cols = window->start_cols;
    const size_t* initial_end_cols = window->end_cols;

    unpacked_float_t* dp_table = malloc(dp_table_height * dp_table_width * sizeof(unpacked_float_t));

    for (size_t col = 0; col < dp_table_width; col++) dp_table[col].f = FLT_MAX;
    for (size_t row = 0; row < dp_table_height; row++) dp_table[row*dp_table_width].f = FLT_MAX;
    dp_table[0].f = 0.0;


    unpacked_float_t diag_cost, up_cost, left_cost;
    float lat_diff, lng_diff, dt;
    size_t prev_start_col = SIZE_MAX;
    size_t prev_end_col = SIZE_MAX;
    size_t start_col, end_col;
    for (size_t row = 1; row <= a_n; row++) {
        start_col = initial_start_cols[row-1];
        end_col = initial_end_cols[row-1];
        for (size_t col = start_col+1; col <= end_col; col++) {
            lat_diff = b_data[2*(col-1) + 0] - a_data[2*(row-1) + 0];
            lng_diff = b_data[2*(col-1) + 1] - a_data[2*(row-1) + 1];
            dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);

            diag_cost.f = ( row == 1 || col == 1 || (prev_start_col <= col-1 && col-1 <= prev_end_col)) ? dp_table[(row-1)*dp_table_width + (col-1)].f : FLT_MAX;
            diag_cost.parts.mantissa &= ~0x0003;
            up_cost.f   = ( row == 1             || (prev_start_col <= col   && col   <= prev_end_col)) ? dp_table[(row-1)*dp_table_width + (col  )].f : FLT_MAX;
            up_cost.parts.mantissa   &= ~0x0003;
            left_cost.f = (             col == 1 || (     start_col <= col-1 && col-1 <=      end_col)) ? dp_table[(row  )*dp_table_width + (col-1)].f : FLT_MAX;
            left_cost.parts.mantissa &= ~0x0003;

            if (diag_cost.f <= up_cost.f && diag_cost.f <= left_cost.f) {
                dp_table[row*dp_table_width + col].f = diag_cost.f + dt;
                dp_table[row*dp_table_width + col].parts.mantissa |= 0x0003;
            }
            else if (up_cost.f <= left_cost.f) {
                dp_table[row*dp_table_width + col].f = up_cost.f + dt;
                dp_table[row*dp_table_width + col].parts.mantissa |= 0x0002;
            }
            else {
                dp_table[row*dp_table_width + col].f = left_cost.f + dt;
                dp_table[row*dp_table_width + col].parts.mantissa |= 0x0001;
            }
        }
        prev_start_col = start_col;
        prev_end_col = end_col;
    }

    size_t u = a_n-1;
    size_t v = b_n-1;
    strided_mask_t* mask = strided_mask_create(a_n, b_n);
    size_t* path_start_cols = mask->start_cols;
    size_t* path_end_cols = mask->end_cols;

    size_t path_len = 1;
    path_start_cols[0] = 0;
    path_end_cols[a_n-1] = b_n-1;

    while(u > 0 && v > 0) {
        path_len++;
        unpacked_float_t cost = dp_table[(u+1)*dp_table_width+(v+1)];
        unsigned int result = cost.parts.mantissa & 0x0003;
        if (result == 0x0003) {
            path_start_cols[u] = v;
            path_end_cols[u-1] = v-1;
            u -= 1;
            v -= 1;
        } else if (result == 0x0002) {
            path_start_cols[u] = v;
            path_end_cols[u-1] = v;
            u -= 1;
        } else {
            // This was a horizontal step. We don't learn anything about the start or end columns.
            v -= 1;
        }
    }

    warp_info_t* warp_info = malloc(sizeof(warp_info_t));
    warp_info->path_mask = mask;
    unpacked_float_t final_cost = dp_table[dp_table_height * dp_table_width - 1];
    final_cost.parts.mantissa &= ~0x0003;
    warp_info->warp_cost=final_cost.f;
    free(dp_table);
    return warp_info;


}


/**
 * Downsamples a stream by a factor of two, averaging consecutive points.
 * Allocates memory. Caller must clean up.
 *
 * [a, b, c, d, e, f] --> [(a+b)/2, (c+d)/2, (e+f)/2]
 * [a, b, c, d, e, f, g] --> [(a+b)/2, (c+d)/2, (e+f)/2]
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

    if (a_n < radius + 4 || b_n < radius + 4) {
        final_warp_info = _full_dtw(a, b);
    } else {
        const stream_t* shrunk_a = _reduce_by_half(a); // Allocates memory
        const stream_t* shrunk_b = _reduce_by_half(b); // Allocates memory
        const warp_info_t* shrunk_warp_info = _fast_dtw(shrunk_a, shrunk_b, radius); // Allocates memory
        stream_destroy(shrunk_a);
        stream_destroy(shrunk_b);
        strided_mask_t* new_window = strided_mask_expand(shrunk_warp_info->path_mask, radius); // Allocates memory
        warp_info_destroy(shrunk_warp_info);
        final_warp_info = _windowed_dtw(a, b, new_window); // Allocates memory
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
    const warp_info_t* warp_info = _full_dtw(a, b);

    // DEBUG PRINT
    //strided_mask_printf(warp_info->path_mask);

    warp_summary_t* final_warp = malloc(sizeof(warp_summary_t));
    final_warp->cost = warp_info->warp_cost;
    size_t* length = malloc(sizeof(size_t));
    final_warp->index_pairs = strided_mask_to_index_pairs(warp_info->path_mask, length);
    final_warp->path_length = *length;
    free(length);
    warp_info_destroy(warp_info);
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
    warp_info_destroy(warp_info);
    return final_warp;
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

    const warp_summary_t* warp_summary = fast_align(a, b, radius);
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

    free(warp_summary->index_pairs);
    free((void*) warp_summary);
    free(a_sparsity);
    free(b_sparsity);

    return (float) (1.0 - total_weight_error / total_weight);
}
