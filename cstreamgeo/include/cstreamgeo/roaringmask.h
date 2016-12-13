#ifndef ROARINGMASK_H
#define ROARINGMASK_H

#include <roaring.h>
#include <stdarg.h>

 /*
 * 1 0 0 0
 * 0 1 0 0
 * 0 0 1 0
 * 0 0 0 1
 *
 * Is represented by (4, 4, roaring([0, 5, 10, 15]))
 *
 * Masks are used internally to represent both DTW Warp Paths and DTW Windows in a space-efficient manner
 */
typedef struct roaring_mask_s {
    size_t n_rows;
    size_t n_cols;
    roaring_bitmap_t* indices;
} roaring_mask_t;

/**
 * Creates a new roaring_mask object with set capacity. All elements default to zero.
 */
roaring_mask_t* roaring_mask_create(const size_t n_rows, const size_t n_cols, const size_t initial_capacity);

/**
 * Creates a new roaring_mask object from a list of ravelled indices.
 */
roaring_mask_t* roaring_mask_create_from_list(const size_t n_rows, const size_t n_cols, const size_t n_indices, ...);

/**
 * Frees the memory allocated by `m`.
 */
void roaring_mask_destroy(const roaring_mask_t* mask);

/**
 * Prints the mask at `m`.
 */
void roaring_mask_printf(const roaring_mask_t* mask);

/**
 * Converts the mask data into a sequence of index pairs:
 * i.e. calling the function with input
 * 1 1 0 0
 * 0 1 0 0
 * 0 0 1 0
 * 0 0 1 1
 * will allocate and return the sequence ((0,0),(0,1),(1,1),(2,2),(3,2),(3,3))
 * This function also sets the value of `path_length` to the number points in the path - in this case, six points.
 * Note: caller is responsible for cleaning up output.
 * Note: this method is only sensible if mask has "monotonically increasing" structure in grid-space.
 *       If the structure of the grid is non-standard, expect this method to return gobbledygook.
 *       TODO: write assertion that a roaring_mask represents a valid path.
 */
size_t* roaring_mask_to_index_pairs(const roaring_mask_t* mask, size_t* path_length);

#endif
