#ifndef STRIDEDMASK_H
#define STRIDEDMASK_H

#include <stdlib.h>
#include <stdarg.h>

/**
 * A strided mask is a sparse binary matrix with specific structural constraints on allowed element configurations.
 * It is used for two primary purposes in this library:
 *   1) It is used to represent "search windows" for windowed dynamic timewarping.
 *   2) It is used to represent "warp paths", the result of a dynamic timewarp computation.
 * When used in form (2) (a "path-mask"), there are additional structural constraints -- see further documentation.
 *
 * Strided masks require the following conditions to hold:
 * 1) All rows must consist of a single "run" of toggled "on" values.
 * 2) The index of the first column that is set "on" in a row is at least as large as the index of the first column in the previous row.
 * 3) The index of the last column that is set "on" in a row is at least as large as the index of the last column in the previous row.
 *
 * Example valid matrices (* = filled, . = unfilled)
 *
 *    0 1 2 3 4 5
 *  0 * * * . . .
 *  1 . * * * . .
 *  2 . * * * . .
 *  3 . . * * * *
 *  4 . . * * * *
 *
 *    0 1 2 3 4 5
 *  0 * * * * . .
 *  1 . * * * * .
 *  2 . . * * * .
 *  3 . . . . * *
 *  4 . . . . . *
 *
 *    0 1 2 3 4 5
 *  0 * * . . . .
 *  1 . * . . . .
 *  2 . . * * * .
 *  3 . . . . . *
 *  4 . . . . . *
 *
 * Example invalid matrices:
 *
 *    0 1 2 3 4 5
 *  0 * * * . * * <-- more than one continuous run
 *  1 . * * * . .
 *  2 . * * * . .
 *  3 . . * * * *
 *  4 . . * * * *
 *
 *    0 1 2 3 4 5
 *  0 * * * . . .
 *  1 . * * * . .
 *  2 . * * * . .
 *  3 * * * * * * <-- start index not monotonically increasing
 *  4 . . * * * *
 *
 *    0 1 2 3 4 5
 *  0 * * * . . .
 *  1 . * * * * .
 *  2 . * * * . . <-- end index not monotonically increasing
 *  3 . . * * * *
 *  4 . . * * * *
 *
 * A path_mask is a special kind of strided mask where
 * 1) start_cols[0] = 0
 * 2) end_cols[n_rows-1] = n_cols-1t
 * 3) end_cols[i-1] <= start_cols[i] <= end_cols[i-1] + 1 for all rows i
 *
 * VALID
 *   0 1 2 3 4 5
 * 0 * . . . . .
 * 1 * * . . . .
 * 2 . * . . . .
 * 3 . * * * . .
 * 4 . . . . * *
 *
 * INVALID
 *   0 1 2 3 4 5
 * 0 * . . . . .
 * 1 * * . . . .
 * 2 . * . . . .
 * 3 . * * * . .
 * 4 . . * * * * <-- overlaps two positions
 *
 * INVALID
 *   0 1 2 3 4 5
 * 0 . * . . . . <-- does not fill upper left corner
 * 1 . * . . . .
 * 2 . * . . . .
 * 3 . * * * . .
 * 4 . . * * * *
 */
typedef struct strided_mask_s {
    size_t n_rows;
    size_t n_cols;
    size_t* start_cols;
    size_t* end_cols;
} strided_mask_t;

/**
 * Creates a new empty strided_mask object with n_rows and n_cols.
 * Allocates memory, caller must handle cleanup.
 * @param n_rows
 * @param n_cols
 * @return An unpopulated strided mask object.
 */
strided_mask_t* strided_mask_create(const size_t n_rows, const size_t n_cols);

/**
 * Creates a strided mask object from a list of start, end column indices
 * Allocates memory, caller must handle cleanup.
 * @param n_rows
 * @param n_cols
 * @param ... Input data; first n_rows of var-args are start_cols, next n_rows of var-args are end_cols
 * @return A populated strided mask object.
 */
strided_mask_t* strided_mask_create_from_list(const size_t n_rows, const size_t n_cols, ...);

/**
 * Destroys the input mask object and frees its memory
 * @param mask
 */
void strided_mask_destroy(const strided_mask_t* mask);

/**
 * Prints the input mask object.
 * @param mask
 */
void strided_mask_printf(const strided_mask_t* mask);

/**
 * This function converts a (presumed) valid path mask to a sequence of index pairs
 * Also sets the value of path length to the number of points in the path.
 * Allocates memory; caller must handle cleanup.
 * @param mask Input math
 * @param path_length Set via side-effects.
 * @return An array of [i_0, j_0, i_1, j_1, ... i_PL-1, j_PL-1] indices into streams i and j that were used to build the mask.
 */
size_t* strided_mask_to_index_pairs(const strided_mask_t* mask, size_t* path_length);

/**
 * Upsamples the input mask by a factor of two. If row_parity or col_parity are set to 1, then we add a row (or col).
 * Finally, dilate the new cells by `radius` in each direction.
 * Allocates and returns a new mask; caller responsible for cleanup.
 * Worked example, with row_parity = 0 and col_parity = 0:
 * Input:
 *
 *   0 1 2 3 4 5
 * 0 . * * . . .
 * 1 . * * * . .
 * 2 . * * * . .
 * 3 . . * * . .
 * 4 . . * * * .
 *
 * First, input is logically upsampled to become:
 *   0 1 2 3 4 5 6 7 8 9 0 1
 * 0 . . * * * * . . . . . .
 * 1 . . * * * * . . . . . .
 * 2 . . * * * * * * . . . .
 * 3 . . * * * * * * . . . .
 * 4 . . * * * * * * . . . .
 * 5 . . * * * * * * . . . .
 * 6 . . . . * * * * . . . .
 * 7 . . . . * * * * . . . .
 * 8 . . . . * * * * * * . .
 * 9 . . . . * * * * * * . .
 *
 * We see row and col parity are set to zero, so we don't need to add another row (or col) at the bottom (or right)
 *
 * Next, that upsampled mask is dilated by a square structuring element parameterized by "radius"
 * Dilation works logically like this:
 * 1) We choose a logical structuring element to dilate by. For now, we only support square dilation.
 * 2) For each cell set in the current strided mask window, set all elements in
 *    its structuring element neighborhood to true
 *
 * Example: suppose a radius-1 dilation. Our upsampled mask becomes extruded to the shape bounded by the @ characters
 *   0 1 2 3 4 5 6 7 8 9 0 1
 * 0 . @ * * * * @ . . . . .
 * 1 . @ * * * * @ @ @ . . .
 * 2 . @ * * * * * * @ . . .
 * 3 . @ * * * * * * @ . . .
 * 4 . @ * * * * * * @ . . .
 * 5 . @ * * * * * * @ . . .
 * 6 . @ @ @ * * * * @ . . .
 * 7 . . . @ * * * * @ @ @ .
 * 8 . . . @ * * * * * * @ .
 * 9 . . . @ * * * * * * @ .
 *
 * Suppose a radius-2 dilation. Our upsampled mask becomes extruded by the . characters:
 *   0 1 2 3 4 5 6 7 8 9 0 1
 * 0 @ @ * * * * @ @ @ @ . .
 * 1 @ @ * * * * @ @ @ @ . .
 * 2 @ @ * * * * * * @ @ . .
 * 3 @ @ * * * * * * @ @ . .
 * 4 @ @ * * * * * * @ @ . .
 * 5 @ @ * * * * * * @ @ . .
 * 6 @ @ @ @ * * * * @ @ @ @
 * 7 @ @ @ @ * * * * @ @ @ @
 * 8 . . @ @ * * * * * * @ @
 * 9 . . @ @ * * * * * * @ @
 *
 * @param mask
 * @param radius
 * @return A new, expanded mask.
 */
strided_mask_t* strided_mask_expand(const strided_mask_t* mask, const int row_parity, const int col_parity, const size_t radius);
#endif
