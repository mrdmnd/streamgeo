#include <cstreamgeo/cstreamgeo.h>
#include <cstreamgeo/stridedmask.h>
#include <cstreamgeo/utilc.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <immintrin.h>
#include <stdbool.h>
#include <string.h>
#include <cstreamgeo/io.h>
#include <stdint.h>

#define PI 3.1415926535f

typedef struct {
    float warp_cost;
    strided_mask_t* path_mask;
} warp_info_t;

void warp_info_destroy(const warp_info_t* warp) {
    strided_mask_destroy(warp->path_mask);
    free((void *) warp);
}


// A method that only returns the *COST* of the alignment (full) -- saves on space if we don't need the path.
// NOTE: allocates space for costs on the stack, assumption is that this will succeed.
int32_t full_dtw_cost(const stream_t* restrict a, const stream_t* restrict b) {
    const int32_t* a_lats = a->lats;
    const int32_t* a_lngs = a->lngs;
    const int32_t* b_lats = b->lats;
    const int32_t* b_lngs = b->lngs;
    const size_t a_n = a->n;
    const size_t b_n = b->n;

    int32_t prev_costs[b_n+1];
    int32_t curr_costs[b_n+1];

    for (size_t col = 0; col <= b_n; col++) {
        prev_costs[col] = INT32_MAX;
    }
    prev_costs[0] = 0;

    int32_t lat_diff, lng_diff, dt;
    int32_t diag_cost, up_cost, left_cost;

    for (size_t row = 0; row < a_n; row++) {
        curr_costs[0] = INT32_MAX;
        for (size_t col = 0; col < b_n; col++) {
            lat_diff = b_lats[col] - a_lats[row];
            lng_diff = b_lngs[col] - a_lngs[row];
            dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
            diag_cost = prev_costs[col];
            up_cost   = prev_costs[col+1];
            left_cost = curr_costs[col];
            if (diag_cost <= up_cost && diag_cost <= left_cost) {
                curr_costs[col+1] = dt + diag_cost;
            } else if (up_cost <= left_cost) {
                curr_costs[col+1] = dt + up_cost;
            } else {
                curr_costs[col+1] = dt + left_cost;
            }
        }
        memcpy(&prev_costs, &curr_costs, (b_n+1)*sizeof(int32_t));
    }
    return curr_costs[b_n];
}

// Idea for optimization: can we hand-unroll this into SSE registers with layout [cell_cost, diag_cost, up_cost, left_cost]
warp_info_t* _full_dtw(const stream_t* restrict a, const stream_t* restrict b) {
    const int32_t* a_lats = a->lats;
    const int32_t* a_lngs = a->lngs;
    const int32_t* b_lats = b->lats;
    const int32_t* b_lngs = b->lngs;
    const size_t a_n = a->n;
    const size_t b_n = b->n;

    int32_t* dp_table = malloc( a_n * b_n * sizeof(int32_t));
    int32_t diag_cost, up_cost, left_cost;
    int32_t lat_diff, lng_diff, dt;
    size_t idx;
    for (size_t row = 0; row < a_n; row++) {
        for (size_t col = 0; col < b_n; col++) {
            lat_diff = b_lats[col] - a_lats[row];
            lng_diff = b_lngs[col] - a_lngs[row];
            dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
            diag_cost = ( row == 0 || col == 0) ? INT32_MAX : dp_table[(row-1)*b_n + (col-1)];
            up_cost =   ( row == 0            ) ? INT32_MAX : dp_table[(row-1)*b_n + (col-0)];
            left_cost = (             col == 0) ? INT32_MAX : dp_table[(row-0)*b_n + (col-1)];
            idx = row*b_n + col;
            if (idx == 0) {
                dp_table[idx] = dt;
            }
            else if (diag_cost <= up_cost && diag_cost <= left_cost) {
                dp_table[idx] = diag_cost + dt;
            }
            else if (up_cost <= left_cost) {
                dp_table[idx] = up_cost + dt;
            }
            else {
                dp_table[idx] = left_cost + dt;
            }
        }
    }
    size_t u = a_n - 1;
    size_t v = b_n - 1;
    strided_mask_t* mask = strided_mask_create(a_n, b_n);
    size_t* start_cols = mask->start_cols;
    size_t* end_cols = mask->end_cols;
    start_cols[0] = 0;
    end_cols[u] = v;
    /* Trace back through the DP table to recover the warp path. */
    while (u > 0 || v > 0) {
        diag_cost = ( u == 0 || v == 0) ? INT32_MAX : dp_table[(u-1)*b_n + (v-1)];
        up_cost   = ( u == 0          ) ? INT32_MAX : dp_table[(u-1)*b_n + (v-0)];
        left_cost = (           v == 0) ? INT32_MAX : dp_table[(u-0)*b_n + (v-1)];
        if (diag_cost <= up_cost && diag_cost <= left_cost) {
            start_cols[u] = v;
            end_cols[u-1] = v-1;
            u -= 1;
            v -= 1;
        } else if (up_cost <= left_cost) {
            start_cols[u] = v;
            end_cols[u-1] = v;
            u -= 1;
        } else {
            v -= 1;
        }
    }
    warp_info_t* warp_info = malloc(sizeof(warp_info_t));
    warp_info->path_mask = mask;
    int32_t final_cost = dp_table[a_n*b_n - 1];
    warp_info->warp_cost=final_cost;
    free(dp_table);
    return warp_info;
}

warp_info_t* _windowed_dtw(const stream_t* restrict a, const stream_t* restrict b, const strided_mask_t* restrict window) {
    const float* a_data = a->data;
    const float* b_data = b->data;
    const size_t a_n = a->n;
    const size_t b_n = b->n;
    const size_t* window_start_cols = window->start_cols;
    const size_t* window_end_cols = window->end_cols;
    float* dp_table = malloc(a_n * b_n * sizeof(float));
    float diag_cost, up_cost, left_cost;
    float lat_diff, lng_diff, dt;
    int prev_start_col = -1;
    int prev_end_col = INT_MAX;
    int start_col;
    int end_col;
    size_t idx;
    for (int row = 0; row < (int) a_n; row++) {
        start_col = (int) window_start_cols[row];
        end_col = (int) window_end_cols[row];
        for (int col = start_col; col <= end_col; col++) {
            lat_diff = b_data[2*col + 0] - a_data[2*row + 0];
            lng_diff = b_data[2*col + 1] - a_data[2*row + 1];
            dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
            diag_cost = ( row == 0 || col == 0 || col-1 < prev_start_col || prev_end_col < col-1) ? FLT_MAX : dp_table[(row-1)*b_n + (col-1)];
            up_cost   = ( row == 0             || col   < prev_start_col || prev_end_col < col  ) ? FLT_MAX : dp_table[(row-1)*b_n + (col  )];
            left_cost = (             col == 0 || col-1 < start_col      || end_col      < col-1) ? FLT_MAX : dp_table[(row  )*b_n + (col-1)];
            idx = row*b_n + col;
            if (idx == 0) {
                dp_table[idx] = dt;
            }
            else if (diag_cost <= up_cost && diag_cost <= left_cost) {
                dp_table[idx] = diag_cost + dt;
            }
            else if (up_cost <= left_cost) {
                dp_table[idx] = up_cost + dt;
            }
            else {
                dp_table[idx] = left_cost + dt;
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
    path_start_cols[0] = 0;
    path_end_cols[u] = v;
    while(u > 0 || v > 0) {
        diag_cost = ( u == 0 || v == 0 || v-1 < window_start_cols[u-1] || window_end_cols[u-1] < v-1) ? FLT_MAX : dp_table[(u-1)*b_n + (v-1)];
        up_cost   = ( u == 0           || v   < window_start_cols[u-1] || window_end_cols[u-1] < v  ) ? FLT_MAX : dp_table[(u-1)*b_n + (v  )];
        left_cost = (           v == 0 || v-1 < window_start_cols[u  ] || window_end_cols[u  ] < v-1) ? FLT_MAX : dp_table[(u  )*b_n + (v-1)];
        if (diag_cost <= up_cost && diag_cost <= left_cost) {
            path_start_cols[u] = v;
            path_end_cols[u-1] = v-1;
            u -= 1;
            v -= 1;
        } else if (up_cost <= left_cost) {
            path_start_cols[u] = v;
            path_end_cols[u-1] = v;
            u -= 1;
        } else {
            v -= 1;
        }
    }
    warp_info_t* warp_info = malloc(sizeof(warp_info_t));
    warp_info->path_mask = mask;
    float final_cost = dp_table[a_n * b_n - 1];
    warp_info->warp_cost=final_cost;
    free(dp_table);
    return warp_info;
}

stream_t* _reduce_by_half(const stream_t* input) {
    const size_t input_n = input->n;
    const float* input_data = input->data;

    stream_t* shrunk_stream = stream_create(input_n / 2);
    float* shrunk_data = shrunk_stream->data;

    for (size_t i = 0; i < 2*(input_n / 2); i+=2) {
        shrunk_data[i + 0] = 0.5f * (input_data[2 * i + 0] + input_data[2 * i + 2]);
        shrunk_data[i + 1] = 0.5f * (input_data[2 * i + 1] + input_data[2 * i + 3]);
    }
    return shrunk_stream;
}

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
        strided_mask_t* new_window = strided_mask_expand(shrunk_warp_info->path_mask, (const int) (a_n % 2),
                                                         (const int) (b_n % 2), radius); // Allocates memory
        stream_destroy(shrunk_a);
        stream_destroy(shrunk_b);
        warp_info_destroy(shrunk_warp_info);
        final_warp_info = _windowed_dtw(a, b, new_window); // Allocates memory
        strided_mask_destroy(new_window);
    }
    return final_warp_info;
}


/* ---------- TOP LEVEL FUNCTIONS, EXPOSED TO API ----------- */


void warp_summary_destroy(const warp_summary_t* ws) {
    free(ws->index_pairs);
    free((void*) ws);
}

warp_summary_t* full_warp_summary_create(const stream_t *a, const stream_t *b) {
    const warp_info_t* warp_info = _full_dtw(a, b);
    warp_summary_t* final_warp = malloc(sizeof(warp_summary_t));
    final_warp->cost = warp_info->warp_cost;
    size_t* length = malloc(sizeof(size_t));
    final_warp->index_pairs = strided_mask_to_index_pairs(warp_info->path_mask, length);
    final_warp->path_length = *length;
    free(length);
    warp_info_destroy(warp_info);
    return final_warp;
}

warp_summary_t* fast_warp_summary_create(const stream_t *a, const stream_t *b, const size_t radius) {
    warp_info_t* warp_info = _fast_dtw(a, b, radius);
    warp_summary_t* final_warp = malloc(sizeof(warp_summary_t));
    final_warp->cost = warp_info->warp_cost;
    size_t* length = malloc(sizeof(size_t));
    final_warp->index_pairs = strided_mask_to_index_pairs(warp_info->path_mask, length);
    final_warp->path_length = *length;
    free(length);
    warp_info_destroy(warp_info);
    return final_warp;
}

float similarity(const stream_t *a, const stream_t *b, const size_t radius) {
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

    float* a_sparsity = stream_sparsity_create(a);
    float* b_sparsity = stream_sparsity_create(b);

    const warp_summary_t* warp_summary = fast_warp_summary_create(a, b, radius);
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

size_t medoid_consensus(const stream_collection_t* input, const int approximate) {
    float cost_matrix[input->n][input->n];

    // Populate cost matrix
    stream_t* first;
    stream_t* second;
    size_t radius = 0;
    if (approximate) {
        for (size_t i = 0; i < input->n; i++) {
            radius = MAX(radius, (size_t) ceilf(powf(input->data[i]->n, 0.25)));
        }
    }

    for (size_t i = 0; i < input->n; i++) {
        first = input->data[i];
        for (size_t j = 0; j <= i; j++) {
            second = input->data[j];
            if (i == j) {
                cost_matrix[i][j] = 0.0;
            } else {
                warp_summary_t* warp_summary = approximate ? fast_warp_summary_create(first, second, radius) : full_warp_summary_create(first, second);
                cost_matrix[i][j] = warp_summary->cost;
                cost_matrix[j][i] = warp_summary->cost;
                warp_summary_destroy(warp_summary);
            }
        }
    }

    // Compute the optimal index.
    size_t best_index = 0;
    float best_cost = FLT_MAX;
    for (size_t i = 0; i < input->n; i++) {
        float accum = 0;
        for (size_t j = 0; j < input->n; j++) {
            accum += cost_matrix[i][j];
        }
        if (accum < best_cost) {
            best_cost = accum;
            best_index = i;
        }
    }
    return best_index;
}

// Mutates/modifies the stream passed as consensus_stream by doing a DBA update.
// If radius != -1, then we're doing an approximate warp summary.
// For each element in the input collection,
void _dba_update(const stream_collection_t* input_collection, stream_t* consensus_stream, const ssize_t radius) {


    return;
}

// Allocates memory for the consensus stream that is returned.
stream_t* dba_consensus(const stream_collection_t* input, const int approximate, const size_t iterations) {
    // Choose a random element of the set to be the initial consensus sequence.
    // Copy the contents from the
    const size_t consensus_length = input->data[0]->n;
    stream_t* consensus = stream_create(consensus_length);

    memcpy(consensus->data, input->data[0]->data, 2*consensus_length);

    ssize_t radius = -1;
    if (approximate) {
        for (size_t i = 0; i < input->n; i++) {
            radius = MAX(radius, (size_t) ceilf(powf(input->data[i]->n, 0.25)));
        }
    }

    const size_t input_collection_size = input->n;
    // tuple_association[i] contains an array of points that are dynamic-timewarp-mapped to point [i] in the consensus stream
    // our standard way of representing a collection of points is ... drum roll ... a stream. Although we don't care about the
    // ordering here, it seemed cleaner than creating a separate type for a collection of unordered points, this is technically
    // an abuse of the stream_collection object.
    stream_collection_t* tuple_association = stream_collection_create(consensus_length);
    for (size_t i = 0; i < consensus_length; i++) {
        tuple_association->data[i] = stream_create(input_collection_size);
    }

    for (size_t i = 0; i < iterations; i++) {
        _dba_update(input, consensus, radius);
    }

    stream_collection_destroy(tuple_association);

    return consensus;
}
