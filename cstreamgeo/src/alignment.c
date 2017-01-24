#include <cstreamgeo/cstreamgeo.h>
#include <cstreamgeo/stridedmask.h>
#include <cstreamgeo/utilc.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define PI 3.1415926535f


typedef struct {
    float warp_cost;
    strided_mask_t* path_mask;
} warp_info_t;


void warp_info_destroy(const warp_info_t* warp) {
    strided_mask_destroy(warp->path_mask);
    free((void *) warp);

}

// THERE EXISTS SOME TERRIBLE SCARY FLOATING POINT HACKERY AHEAD
// We store *step-direction* in the low bits of the mantissa - this is a space/accuracy trade off.
// 11 means "diag", 10 means "up", 01 means "left"
// bitwise AND with ~0x0003 (11111111111111100 mask) lets us recover the true cost from the dp table.
typedef union {
    float f;
    struct {
        unsigned int mantissa : 23;
        unsigned int exponent : 8;
        unsigned int sign : 1;
    } parts;
} unpacked_float_t;

// A method that only returns the *COST* of the alignment (full) -- saves on space if we don't need the path.
float full_dtw_cost(const stream_t* restrict a, const stream_t* restrict b) {
    const float* a_data = a->data;
    const float* b_data = b->data;
    const size_t a_n = a->n;
    const size_t b_n = b->n;
    float prev_costs[b_n+1];
    float curr_costs[b_n+1];
    for (size_t col = 0; col <= b_n; col++) {
        prev_costs[col] = FLT_MAX;
    }
    prev_costs[0] = 0;
    float lat_diff, lng_diff, dt;
    float diag_cost, up_cost, left_cost;
    for (size_t row = 0; row < a_n; row++) {
        curr_costs[0] = FLT_MAX;
        for (size_t col = 0; col < b_n; col++) {
            lat_diff = b_data[2*col + 0] - a_data[2*row + 0];
            lng_diff = b_data[2*col + 1] - a_data[2*row + 1];
            dt =  (lng_diff * lng_diff) + (lat_diff * lat_diff);
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
        memcpy(&prev_costs, &curr_costs, (b_n+1)*sizeof(float));
    }
    return curr_costs[b_n];
}

/* OPEN QUESTION: DOES IT MAKE SENSE TO DO A BOUNDARY EDGE ON LEFT/UP EDGE TO SAVE CONDITIONAL BRANCHING? */
warp_info_t* _full_dtw(const stream_t* restrict a, const stream_t* restrict b) {
    const float* a_data = a->data;
    const float* b_data = b->data;

    const size_t a_n = a->n;
    const size_t dp_rows = a_n + 1;

    const size_t b_n = b->n;
    const size_t dp_cols = b_n + 1;

    float* dp_table = malloc( dp_rows * dp_cols * sizeof(float));
    // Left boundary
    for (size_t i = 0; i < dp_rows; i++) {
        dp_table[i * dp_cols + 0] = FLT_MAX;
    }
    // Upper boundary
    for (size_t j = 0; j < dp_cols; j++) {
        dp_table[0 * dp_cols + j] = FLT_MAX;
    }
    float diag_cost, up_cost, left_cost;
    float lat_diff, lng_diff, dt;
    for (size_t row = 1; row < dp_rows; row++) {
        for (size_t col = 1; col < dp_cols; col++) {
            lat_diff = b_data[2*col + 0] - a_data[2*row + 0];
            lng_diff = b_data[2*col + 1] - a_data[2*row + 1];
            dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
            diag_cost = dp_table[(row-1)*b_n + (col-1)];
            up_cost   = dp_table[(row-1)*b_n + (col  )];
            left_cost = dp_table[(row  )*b_n + (col-1)];

            if (diag_cost <= up_cost && diag_cost <= left_cost) {
                dp_table[row*b_n + col] = diag_cost + dt;
            }
            else if (up_cost <= left_cost) {
                dp_table[row*b_n + col] = up_cost + dt;
            }
            else {
                dp_table[row*b_n + col] = left_cost + dt;
            }
        }
    }

    size_t u = a_n;
    size_t v = b_n;
    strided_mask_t* mask = strided_mask_create(a_n, b_n);
    size_t* start_cols = mask->start_cols;
    size_t* end_cols = mask->end_cols;
    start_cols[0] = 0;
    end_cols[u-1] = v-1;
    /* Trace back through the DP table to recover the warp path. */
    while (u > 0 && v > 0) {
        diag_cost = dp_table[(u-1)*b_n + (v-1)];
        up_cost   = dp_table[(u-1)*b_n + (v  )];
        left_cost = dp_table[(u  )*b_n + (v-1)];
        if (diag_cost <= up_cost && diag_cost <= left_cost) {
            start_cols[u-1] = v-1;
            end_cols[u-2] = v-2;
            u -= 1;
            v -= 1;
        } else if (up_cost <= left_cost) {
            start_cols[u-1] = v-1;
            end_cols[u-2] = v-1;
            u -= 1;
        } else {
            v -= 1;
        }
    }

    warp_info_t* warp_info = malloc(sizeof(warp_info_t));
    warp_info->path_mask = mask;
    float final_cost = dp_table[a_n*b_n - 1];
    warp_info->warp_cost=final_cost;
    free(dp_table);
    return warp_info;
}

warp_info_t* _windowed_dtw(const stream_t* restrict a, const stream_t* restrict b, const strided_mask_t* restrict window) {
    const float* a_data = a->data;
    const float* b_data = b->data;
    const size_t a_n = a->n;
    const size_t b_n = b->n;
    const size_t* initial_start_cols = window->start_cols;
    const size_t* initial_end_cols = window->end_cols;
    unpacked_float_t* dp_table = malloc(a_n * b_n * sizeof(unpacked_float_t));
    unpacked_float_t diag_cost, up_cost, left_cost;
    float lat_diff, lng_diff, dt;
    int prev_start_col = -1;
    int prev_end_col = INT_MAX;
    int start_col;
    int end_col;
    for (int row = 0; row < (int) a_n; row++) {
        start_col = (int) initial_start_cols[row];
        end_col = (int) initial_end_cols[row];
        for (int col = start_col; col <= end_col; col++) {
            lat_diff = b_data[2*col + 0] - a_data[2*row + 0];
            lng_diff = b_data[2*col + 1] - a_data[2*row + 1];
            dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
            diag_cost.f = ( row == 0 || col == 0 || col-1 < prev_start_col || prev_end_col < col-1) ? FLT_MAX : dp_table[(row-1)*b_n + (col-1)].f ;
            diag_cost.parts.mantissa &= ~0x0003;
            up_cost.f   = ( row == 0             || col   < prev_start_col || prev_end_col < col  ) ? FLT_MAX : dp_table[(row-1)*b_n + (col  )].f ;
            up_cost.parts.mantissa   &= ~0x0003;
            left_cost.f = (             col == 0 || col-1 < start_col      || end_col      < col-1) ? FLT_MAX : dp_table[(row  )*b_n + (col-1)].f ;
            left_cost.parts.mantissa &= ~0x0003;
            if (row == 0 && col == 0) {
                dp_table[0].f = 0 + dt;
                dp_table[row*b_n + col].parts.mantissa &= ~0x0003;
                dp_table[0].parts.mantissa |= 0x0003;
            }
            else if (diag_cost.f <= up_cost.f && diag_cost.f <= left_cost.f) {
                dp_table[row*b_n + col].f = diag_cost.f + dt;
                dp_table[row*b_n + col].parts.mantissa &= ~0x0003;
                dp_table[row*b_n + col].parts.mantissa |= 0x0003;
            }
            else if (up_cost.f <= left_cost.f) {
                dp_table[row*b_n + col].f = up_cost.f + dt;
                dp_table[row*b_n + col].parts.mantissa &= ~0x0003;
                dp_table[row*b_n + col].parts.mantissa |= 0x0002;
            }
            else {
                dp_table[row*b_n + col].f = left_cost.f + dt;
                dp_table[row*b_n + col].parts.mantissa &= ~0x0003;
                dp_table[row*b_n + col].parts.mantissa |= 0x0001;
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
    path_end_cols[u] = v;

    while(u > 0 || v > 0) {
        path_len++;
        unpacked_float_t cost = dp_table[u*b_n + v];
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
            v -= 1;
        }
    }
    warp_info_t* warp_info = malloc(sizeof(warp_info_t));
    warp_info->path_mask = mask;
    unpacked_float_t final_cost = dp_table[a_n * b_n - 1];
    final_cost.parts.mantissa &= ~0x0003;
    warp_info->warp_cost=final_cost.f;
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
    if (approximate) {
        size_t radius = 0;
        for (size_t i = 0; i < input->n; i++) {
            radius = MAX(radius, (size_t)ceilf( powf(input->data[i]->n, 0.25) ));
        }

        for (size_t i = 0; i < input->n; i++) {
            first = input->data[i];
            for (size_t j = 0; j <= i; j++) { // TODO: check if this is a bug; and if we want j < i as the test
                second = input->data[j];
                if (i == j) {
                    cost_matrix[i][j] = 0.0;
                } else {
                    warp_summary_t* warp_summary = fast_warp_summary_create(first, second, radius);
                    cost_matrix[i][j] = warp_summary->cost;
                    cost_matrix[j][i] = warp_summary->cost;
                    warp_summary_destroy(warp_summary);
                }
            }
        }
    } else {
        for (size_t i = 0; i < input->n; i++) {
            first = input->data[i];
            for (size_t j = 0; j <= i; j++) {
                second = input->data[j];
                if (i == j) {
                    cost_matrix[i][j] = 0.0;
                } else {
                    warp_summary_t* warp_summary = full_warp_summary_create(first, second);
                    cost_matrix[i][j] = warp_summary->cost;
                    cost_matrix[j][i] = warp_summary->cost;
                    warp_summary_destroy(warp_summary);
                }
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

/*
stream_t* dba_consensus(const stream_collection_t* input, const int approximate) {
    printf("NOT YET IMPLEMENTED\n");
    return NULL;
}*/
