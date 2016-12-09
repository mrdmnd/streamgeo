#include <cstreamgeo/dbg.h>
#include <cstreamgeo/roaringmask.h>

/**
 * Initialization for a roaring mask object
 * @param n_rows Number of rows
 * @param n_cols Number of columns
 * @param initial_capacity Pre-allocates space for indexes, because we store non-zero entries only.
 * @return roaring mask object
 */
roaring_mask_t* roaring_mask_create(const size_t n_rows, const size_t n_cols, const size_t initial_capacity) {
    roaring_mask_t* mask = malloc(sizeof(roaring_mask_t));
    mask->n_rows = n_rows;
    mask->n_cols = n_cols;
    mask->indices = roaring_bitmap_create_with_capacity(initial_capacity);
    return mask;
}

/**
 * Variadic list-based initialization for a roaring mask object.
 * @param n_rows Number of rows
 * @param n_cols Number of columns
 * @param ... Input indices for the roaring mask.
 */
roaring_mask_t* roaring_mask_create_from_list(const size_t n_rows, const size_t n_cols, const size_t n_indices, ...) {
    va_list args;
    va_start(args, n_indices);
    roaring_mask_t* mask = roaring_mask_create(n_rows, n_cols, n_indices);
    roaring_bitmap_t* indices = mask->indices;
    for (size_t i = 0; i < n_indices; i++) {
        roaring_bitmap_add(indices, va_arg(args, uint32_t));
    }
    va_end(args);
    return mask;
}

/**
 * Roaring mask destructor
 * @param mask Mask to destroy
 */
void roaring_mask_destroy(const roaring_mask_t* mask) {
    if (mask) {
        roaring_bitmap_free(mask->indices);
        free(mask);
    } else {
        log_warn("Attempted to free a null mask pointer.");
    }
}

/**
 * Debug tool for visualization of a roaring_mask object
 * @param mask Mask to vizualize
 */
void roaring_mask_printf(const roaring_mask_t* mask) {
    roaring_bitmap_t* indices = mask->indices;
    size_t n_rows = mask->n_rows;
    size_t n_cols = mask->n_cols;

    for (int i = 0; i < n_rows; i++) {
        for (int j = 0; j < n_cols; j++) {
            roaring_bitmap_contains(indices, i * n_cols + j) ? printf("1 ") : printf("0 ");
        }
        printf("\n");
    }
}


/**
 * Small data type to hold metadata for converting a path mask of indices (ravelled) to an array of index tuples
 */
typedef struct {
    size_t width;
    size_t* current_point;
    size_t* current_path;
} _iter_arg_t;

/**
 * Iterator fn for roaring_mask_to_index_pairs that recovers i, j index tuples from ravelled indices
 * @param index
 * @param arg
 */
bool _create_tuple_from_index(uint32_t index, void* arg) {
    _iter_arg_t* a = (_iter_arg_t*) arg;
    size_t width = a->width;
    size_t* current_point = a->current_point;
    size_t* current_path = a->current_path;

    size_t i = index / width;
    size_t j = index % width;
    current_path[*current_point * 2 + 0] = i;
    current_path[*current_point * 2 + 1] = j;
    *current_point += 1;
}

/**
 * Converts a path mask stored as a roaring_mask object into a human-readable sequence of (i, j) index pairs
 * This is how we recover the optimal alignment returned by the timewarp methods.
 * @param mask Input mask to decode
 * @param path_length Output number of points in the optimal alignment path
 * @return Output sequence of (i, j) index pairs stored [i0, j0, i1, j1, ... iN, jN]
 */
size_t* roaring_mask_to_index_pairs(const roaring_mask_t* mask, size_t* path_length) {
    size_t total_points = roaring_bitmap_get_cardinality(mask);
    *path_length = total_points;

    size_t* path = malloc(2 * sizeof(size_t) * total_points);

    _iter_arg_t* arg = malloc(sizeof(_iter_arg_t));
    arg->width = mask->n_cols;
    arg->current_point = 0;
    arg->current_path = path;

    roaring_iterate(mask->indices, _create_tuple_from_index, arg);
    free(arg);

    return path;
}
