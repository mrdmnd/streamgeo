#include <similarity.h>
/**
 * The core routines for computing {fast/full} dynamic time warp distance between stream buffers.
 */

/**
 * A small data type for use in the evaluation of a single dynamic programming cell call.
 */
typedef struct {
    const float* a;
    const float* b;
    int w_n_cols;
    int b_dp_table_size;
    float* dp_table;
} eval_dp_cell_arg;

/**
 * A helper method for evaluating a single dynamic programming call.
 * @param index A ravelled value (i * n_cols + j) to evaluate the cost at
 * @param arg Packed tuple containing stream data and metadata
 */
void _eval_dp_cell(uint32_t index, void* arg) {
    // Unpack arg tuple
    const float* a = ((eval_dp_cell_arg*)arg)->a;
    const float* b = ((eval_dp_cell_arg*)arg)->b;
    int w_n_cols = ((eval_dp_cell_arg*)arg)->w_n_cols;
    int b_dp_table_size = ((eval_dp_cell_arg*)arg)->b_dp_table_size;
    float* dp_table = ((eval_dp_cell_arg*)arg)->dp_table;

    int i = index / w_n_cols;
    int j = index % w_n_cols;
    float lat_diff = b[2*j] - a[2*i];
    float lng_diff = b[2*j+1] - a[2*i+1];
    float dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
    int m = (i+1) * b_dp_table_size + (j+1);
    float diag_cost  = dp_table[m - b_dp_table_size - 1];
    float up_cost    = dp_table[m - b_dp_table_size];
    float left_cost  = dp_table[m - 1];
    if (diag_cost <= up_cost && diag_cost <= left_cost) {
        dp_table[m] = diag_cost + dt;
    } else if (up_cost <= left_cost) {
        dp_table[m] = up_cost + dt;
    } else {
        dp_table[m] = left_cost + dt;
    }
}

/**
 * A top-level method that computes an alignment mask and DTW distance between two streams.
 * If passed a window mask in parameter `w`, this method only evalutes the DTW alignment on the cells in the window.
 * If passed a null window mask, this method evalutes the DTW alignment for all cells (quadratic behavior).
 * @param a_n Number of points in stream buffer `a`
 * @param a Stream buffer data for first stream
 * @param b_n Number of points in stream buffer `b`
 * @param b Stream buffer data for second stream
 * @param w Parameter (can be null) containing a window mask to evaluate DTW on.
 * @param p Output parameter containing warp path for optimal alignment.
 * @return Cost of alignment between stream `a` and stream `b`
 */
float _dtw(const int a_n, const float* a, const int b_n, const float* b, const RoaringMask* w, RoaringMask* p) {
    // Table is one index larger than stream on each side to provide easy padding logic.
    int a_dp_table_size = a_n + 1;
    int b_dp_table_size = b_n + 1;

    // Set up and value-initialize DP table
    float* dp_table = malloc(a_dp_table_size * b_dp_table_size * sizeof(float));
    assert(dp_table != NULL);
    int dp_size = a_dp_table_size*b_dp_table_size;
    for(int n = 0; n < dp_size; ++n) {
        dp_table[n] = FLT_MAX;
    }
    dp_table[0] = 0.0;

    if (w == NULL) {
        // Full DTW if window parameter is passed in as null.
        // A bit of code repetition with _eval_dp_cell(), but this is a significant performance optimization -
        // not worth creating a "full window" and using roaring_iterate over that window due to a lot of overhead.
        for (int i = 0; i < a_n; i++) {
            for (int j = 0; j < b_n; j++) {
                float lat_diff = b[2*j] - a[2*i];
                float lng_diff = b[2*j+1] - a[2*i+1];
                float dt = (lng_diff * lng_diff) + (lat_diff * lat_diff);
                int m = (i+1) * b_dp_table_size + (j+1);
                float diag_cost  = dp_table[m - b_dp_table_size - 1];
                float up_cost    = dp_table[m - b_dp_table_size];
                float left_cost  = dp_table[m - 1];
                if (diag_cost <= up_cost && diag_cost <= left_cost) {
                    dp_table[m] = diag_cost + dt;
                } else if (up_cost <= left_cost) {
                    dp_table[m] = up_cost + dt;
                } else {
                    dp_table[m] = left_cost + dt;
                }
            }
        }
    } else {
        // DTW evaluation only on window cells otherwise
        eval_dp_cell_arg* edca = malloc(sizeof(eval_dp_cell_arg));
        edca->a = a;
        edca->b = b;
        edca->w_n_cols = w->n_cols;
        edca->b_dp_table_size = b_dp_table_size;
        edca->dp_table = dp_table;
        // This method takes a function pointer (in this case, _eval_dp_cell) with signature (index, arg)
        // and an argument to pass into that fn (in this case, edca), then
        // applies the function _eval_dp_cell to every single cell contained in our window.
        // This is the canonical way of iterating over a roaring_bitmask container.
        roaring_iterate(w->indices, _eval_dp_cell, edca);
        free(edca);
    }

    // Finally, trace back through the path defined by the DP table.
    // I tested storing this path during DP table creation, but it turned out to be faster in every benchmark to
    // recreate the path ex-post-facto rather than touching extra memory.
    int u = a_n;
    int v = b_n;
    while (u > 0 && v > 0) {
        int idx = (u-1)*b_n + (v-1);
        roaring_bitmap_add(p->indices, idx);
        float diag_cost  = dp_table[(u-1)*b_dp_table_size + (v-1)];
        float up_cost    = dp_table[(u-1)*b_dp_table_size + (v  )];
        float left_cost  = dp_table[(u  )*b_dp_table_size + (v-1)];
        if ( diag_cost <= up_cost && diag_cost <= left_cost ) {
            u -= 1;
            v -= 1;
        } else if ( up_cost <= left_cost ) {
            u -= 1;
        } else {
            v -= 1;
        }
    }
    float end_cost = dp_table[a_dp_table_size*b_dp_table_size - 1];
    free(dp_table);
    return end_cost;
}


/**
 * A small data type used in the _expand_cell method
 */
typedef struct {
    int radius;
    int shrunk_cols;
    RoaringMask* window_out;
} expand_cell_arg;

/**
 * Mutates the window_out member in the `arg` parameter by "scaling up" a set bit, then extruding it by a radius
 * Example:
 * If radius = 0,
 * . . . . . . .
 * . . . . . . .
 * . . . . . @ .
 * . . . . . . .
 * gets scaled up to
 * . . . . . . . . . . . . . .
 * . . . . . . . . . . . . . .
 * . . . . . . . . . . . . . .
 * . . . . . . . . . . . . . .
 * . . . . . . . . . . @ @ . .
 * . . . . . . . . . . @ @ . .
 * . . . . . . . . . . . . . .
 * . . . . . . . . . . . . . .
 * If radius = 1,
 * . . . . . . . . . . . . . .
 * . . . . . . . . . . . . . .
 * . . . . . . . . . . . . . .
 * . . . . . . . . . @ @ @ @ .
 * . . . . . . . . . @ @ @ @ .
 * . . . . . . . . . @ @ @ @ .
 * . . . . . . . . . @ @ @ @ .
 * . . . . . . . . . . . . . .
 * @param value Ravelled index to scale up and extrude
 * @param arg Metadata on extrusion (radius, target size, and output window mask)
 */
void _expand_cell(uint32_t value, void* arg) {
    int radius = ((expand_cell_arg*)arg)->radius;
    int shrunk_cols   = ((expand_cell_arg*)arg)->shrunk_cols;
    RoaringMask* window_out = ((expand_cell_arg*)arg)->window_out;

    int p = 2*(value / shrunk_cols); int q = 2*(value % shrunk_cols);
    for(int i = 0; i <= 1; i++) {
        for(int j = 0; j <= 1; j++) {
             for (int x = -radius; x <= radius; x++) {
                for (int y = -radius; y <= radius; y++) {
                    int first = (p + x + i);
                    int second = (q + y + j);
                    if (0 <= first && first < window_out->n_rows && 0 <= second && second < window_out->n_cols) {
                        int idx = first *  window_out->n_cols + second;
                        roaring_bitmap_add(window_out->indices, idx);
                    }
                }
            }
        }
    }
}

/**
 * Scales up a path mask by a constant radius r. Used by the expansion phase of the fast_dtw algorithm
 * Example:
 * If radius = 0,
 * @ @ . . . . .
 * . . @ @ . . .
 * . . . . @ @ .
 * . . . . . . @
 * gets scaled up to
 * @ @ @ @ . . . . . . . . . .
 * @ @ @ @ . . . . . . . . . .
 * . . . . @ @ @ @ . . . . . .
 * . . . . @ @ @ @ . . . . . .
 * . . . . . . . . @ @ @ @ . .
 * . . . . . . . . @ @ @ @ . .
 * . . . . . . . . . . . . @ @
 * . . . . . . . . . . . . @ @
 * If radius = 1,
 * @ @ @ @ @ . . . . . . . . .
 * @ @ @ @ @ @ @ @ @ . . . . .
 * @ @ @ @ @ @ @ @ @ . . . . .
 * . . . @ @ @ @ @ @ @ @ @ @ .
 * . . . @ @ @ @ @ @ @ @ @ @ .
 * . . . . . . . @ @ @ @ @ @ @
 * . . . . . . . @ @ @ @ @ @ @
 * . . . . . . . . . . . @ @ @
 * @param shrunk_path Input path to scale up and extrude
 * @param radius Parameter controlling extrusion amount
 * @param window_out Output parameter for new path
 */
void _expand_window(const RoaringMask shrunk_path, const int radius, RoaringMask* window_out) {
    expand_cell_arg* eca = malloc(sizeof(expand_cell_arg));
    eca->radius = radius;
    eca->shrunk_cols = shrunk_path.n_cols;
    eca->window_out = window_out;
    roaring_iterate(shrunk_path.indices, _expand_cell, eca);
    free(eca);
    // This is a small perf optimization; our bitmaps are generally "banded" and have lots of "runs" as such.
    // This line does run-length encoding on the bitmap where applicable.
    roaring_bitmap_run_optimize(window_out->indices);
}

/**
 * Small data type to hold metadata for converting a path mask of indices (ravelled) to an array of index tuples
 */
typedef struct {
    int width;
    int* current_n_points;
    int* current_points;
} create_tuple_from_index_arg;

/**
 * Iterator fn for _pathmask_to_tuplearray that recovers i, j index tuples from ravelled indices
 * @param index
 * @param arg
 */
void _create_tuple_from_index(uint32_t index, void* arg) {
    // Insert into path
    int width = ((create_tuple_from_index_arg*)arg)->width;
    int* current_n_points = ((create_tuple_from_index_arg*)arg)->current_n_points;
    int* current_path = ((create_tuple_from_index_arg*)arg)->current_points;
    int i = index / width;
    int j = index % width;
    current_path[*current_n_points * 2 + 0] = i;
    current_path[*current_n_points * 2 + 1] = j;
    *current_n_points += 1;
}

/**
 * Converts a path mask stored as a RoaringMask object into a human-readable sequence of (i, j) index pairs
 * This is how we recover the optimal alignment returned by the timewarp methods.
 * @param mask Input mask to decode
 * @param path_n Output number of points in the optimal alignment path
 * @param path Output sequence of (i, j) index pairs stored [i0, j0, i1, j1, ... iN, jN]
 */
void _pathmask_to_tuplearray(const RoaringMask* mask, int* path_n, int* path) {
    // Requires path to be large enough to hold a_n + b_n - 1 points
    // Must call this with path_n starting out as a pointer pointing to zero.
    assert(*path_n == 0);
    create_tuple_from_index_arg* arg = malloc(sizeof(create_tuple_from_index_arg));
    arg->width = mask->n_cols;
    arg->current_n_points = path_n;
    arg->current_points = path;
    roaring_iterate(mask->indices, _create_tuple_from_index, arg);
    free(arg);
}

/**
 * Downsamples a stream by a factor of two, averaging consecutive points.
 * [a, b, c, d, e, f] --> [(a+b)/2, (c+d)/2, (e+f)/2]
 * @param input_n Number of points in input buffer
 * @param input Data buffer containing original stream
 * @param output Data buffer containing down-sampled stream
 */
void _reduce_by_half(const int input_n, const float* input, float* output) {
    for (int i = 0; i < input_n-1; i++) {
        float lat1 = input[2*i];
        float lng1 = input[2*i+1];
        float lat2 = input[2*i+2];
        float lng2 = input[2*i+3];
        if (i % 2 == 0) {
            output[i] = 0.5f*(lat1+lat2);
            output[i+1] = 0.5f*(lng1+lng2);
        }
    }
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
float _fast_dtw(const int a_n, const float* a, const int b_n, const float* b, const int radius, RoaringMask* path) {
    int min_size = radius + 2;
    if (a_n <= min_size || b_n <= min_size) {
        return _dtw(a_n, a, b_n, b, NULL, path); // Also sets path output variable through side-effects.
    } else {
        int shrunk_a_n = a_n / 2; // requires integer division
        int shrunk_b_n = b_n / 2; // requires integer division
        float* shrunk_a = malloc(2 * shrunk_a_n * sizeof(float));
        float* shrunk_b = malloc(2 * shrunk_b_n * sizeof(float));

        _reduce_by_half(a_n, a, shrunk_a);
        _reduce_by_half(b_n, b, shrunk_b);

        // Worse case path is all the way down, then all the way right (minus one because this double-counts corner).
        RoaringMask* shrunk_path = roaring_mask_create( shrunk_a_n, shrunk_b_n, shrunk_a_n + shrunk_b_n - 1);

        // Populate a shrunk path to feed into the expand_window fn
        _fast_dtw(shrunk_a_n, shrunk_a, shrunk_b_n, shrunk_b, radius, shrunk_path);
        free(shrunk_a);
        free(shrunk_b);
        // It takes some cleverness to figure out the maximum number of cells that we'll see
        // The worst case is when a_n = b_n, and the shrunk_path passes exactly on the diagonal
        // see page six of http://cs.fit.edu/~pkc/papers/tdm04.pdf for more
        int max_window_size = (4 * radius + 3) * MAX(a_n, b_n);
        RoaringMask* new_window = roaring_mask_create(a_n, b_n, max_window_size);
        _expand_window(*shrunk_path, radius, new_window);
        roaring_mask_destroy(shrunk_path);
        float result = _dtw(a_n, a, b_n, b, new_window, path);
        roaring_mask_destroy(new_window);
        return result;
    }
}

/**
 * Top level call to compute alignment between streams. This is what external interfaces should call.
 * A thin wrapper around _fast_dtw and _pathmask_to_tuplearray.
 * @param a_n Number of points in the first stream
 * @param a Stream buffer for the first stream
 * @param b_n Number of points in the second stream
 * @param b Stream buffer for the second stream
 * @param radius Amount to extrude shrunk path by: set to one by default. Larger values are more exhaustive search.
 * @param path_n Output parameter containing number of points in the returned optimal alignment path
 * @param path Output parameter containing optimal alignment path
 * @return Alignment cost of optimal alignment path
 */
float align(const int a_n, const float* a, const int b_n, const float* b, const int radius, int* path_n, int* path) {
    RoaringMask* mask = roaring_mask_create(a_n, b_n, a_n + b_n - 1);
    float cost = _fast_dtw(a_n, a, b_n, b, radius, mask);
    int* n_points_accum = malloc(sizeof(int));
    *n_points_accum = 0;
    _pathmask_to_tuplearray(mask, n_points_accum, path);
    *path_n = *n_points_accum;
    free(n_points_accum);
    roaring_mask_destroy(mask);
    return cost;
}

/**
 * Small data type for passing in metadata for iterative_similarity calls.
 */
typedef struct {
    int a_n;
    const float* a;
    int b_n;
    const float* b;
    int n_cols;
    float min_distance;
    float* a_sparsity;
    float* b_sparsity;
    float* total_weight;
    float* total_weight_error;
} iterative_similarity_arg;

/**
 * A helper function for iterating over stream data to compute a similarity metric
 * @param value Ravelled index: i * n_cols + j
 * @param arg Metadata on streams
 */
void _iterative_similarity(uint32_t value, void* arg) {
    const int a_n = ((iterative_similarity_arg*)arg)->a_n;
    const float* a = ((iterative_similarity_arg*)arg)->a;
    const int b_n = ((iterative_similarity_arg*)arg)->b_n;
    const float* b = ((iterative_similarity_arg*)arg)->b;
    int n_cols = ((iterative_similarity_arg*)arg)->n_cols;
    float min_distance = ((iterative_similarity_arg*)arg)->min_distance;
    float* a_sparsity = ((iterative_similarity_arg*)arg)->a_sparsity;
    float* b_sparsity = ((iterative_similarity_arg*)arg)->b_sparsity;
    float* total_weight = ((iterative_similarity_arg*)arg)->total_weight;
    float* total_weight_error = ((iterative_similarity_arg*)arg)->total_weight_error;

    int i = value / n_cols;
    int j = value % n_cols;
    float lat = b[2*j] - a[2*i];
    float lng = b[2*j+1] - a[2*i+1];
    float cost = sqrt((lng * lng) + (lat * lat));
    float unitless_cost = cost / min_distance;
    float error = 1.0 - exp(-unitless_cost*unitless_cost);

    // Weight start/end less than the middle, weight sparse points less
    float sparsity_weight_a = a_sparsity[i];
    float sparsity_weight_b = b_sparsity[j];
    float positional_weight_a = 0.1 + 0.9 * sin(M_PI * i / a_n);
    float positional_weight_b = 0.1 + 0.9 * sin(M_PI * j / b_n);
    float weight = positional_weight_a * positional_weight_b * sparsity_weight_a * sparsity_weight_b;
    *total_weight += weight;
    *total_weight_error += (weight * error);
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
float similarity(const int a_n, const float* a, const int b_n, const float* b, const int radius) {
    if (a_n < 2 || b_n < 2) {
        return 0.0;
    }
    // Horribly mismatched distance ratio
    float a_stream_distance = stream_distance(a_n, a);
    float b_stream_distance = stream_distance(b_n, b);
    float ratio = a_stream_distance / b_stream_distance;
    if (ratio < 0.4 || ratio > 2.5) {
        printf("Mismatched distance ratio = %f means similarity is zero.\n", ratio);
        return 0.0;
    }
    // If start/mid/endpoints are further apart than 30% of min distance, fail.
    float min_distance = MIN(a_stream_distance, b_stream_distance);
    // Start point
    float lat_diff = b[2*(0)] - a[2*(0)];
    float lng_diff = b[2*(0)+1] - a[2*(0)+1];
    if (sqrt((lng_diff * lng_diff) + (lat_diff * lat_diff)) > 0.3 * min_distance) return 0.0;
    // Midpoint
    lat_diff = b[2 * (b_n / 2)] - a[2 * (a_n / 2)];
    lng_diff = b[2 * (b_n / 2) + 1] - a[2 * (a_n / 2) + 1];
    if (sqrt((lng_diff * lng_diff) + (lat_diff * lat_diff)) > 0.3 * min_distance) return 0.0;
    // Endpoint
    lat_diff = b[2 * (b_n - 1)] - a[2 * (a_n - 1)];
    lng_diff = b[2 * (b_n - 1) + 1] - a[2 * (a_n - 1) + 1];
    if (sqrt((lng_diff * lng_diff) + (lat_diff * lat_diff)) > 0.3 * min_distance) return 0.0;

    float* a_sparsity = malloc(a_n * sizeof(float));
    float* b_sparsity = malloc(b_n * sizeof(float));
    stream_sparsity(a_n, a, a_sparsity);
    stream_sparsity(b_n, b, b_sparsity);


    RoaringMask* path = roaring_mask_create(a_n, b_n, a_n + b_n - 1);
    _fast_dtw(a_n, a, b_n, b, radius, path);
    float* total_weight = malloc(sizeof(float));
    float* total_weight_error = malloc(sizeof(float));
    *total_weight = 0;
    *total_weight_error = 0;

    iterative_similarity_arg* arg = malloc(sizeof(iterative_similarity_arg));
    arg->a_n = a_n;
    arg->a = a;
    arg->b_n = b_n;
    arg->b = b;
    arg->n_cols = path->n_cols;
    arg->total_weight = total_weight;
    arg->total_weight_error = total_weight_error;
    arg->a_sparsity = a_sparsity;
    arg->b_sparsity = b_sparsity;
    arg->min_distance = min_distance;

    roaring_iterate(path->indices, _iterative_similarity, arg);

    free(arg);
    roaring_mask_destroy(path);
    free(a_sparsity);
    free(b_sparsity);
    float ret = 1.0 - *total_weight_error / *total_weight;
    free(total_weight);
    free(total_weight_error);
    return ret;
}
