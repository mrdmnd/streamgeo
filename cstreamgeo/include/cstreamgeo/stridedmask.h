#ifndef STRIDEDMASK_H
#define STRIDEDMASK_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

/*
 A strided mask is a sparse binary matrix of very specific form:
 This is the form of the "search window" for dynamic timewarping during the "expand cells" phase.
 1) All rows consist of a single "run" of "on" values.
 2) The first index that is set "on" in a row is at least as large as the starting "on" index in the previous row.
 3) The last index that is set "on" in a row is at least as large as the ending "on" index in the previous row.

 Example valid matrices (* = filled, . = unfilled)

  0 1 2 3 4 5
0 * * * . . .
1 . * * * . .
2 . * * * . .
3 . . * * * *
4 . . * * * *

  0 1 2 3 4 5
0 * * * * . .
1 . * * * * .
2 . . * * * .
3 . . . . * *
4 . . . . . *

  0 1 2 3 4 5
0 * * . . . .
1 . * . . . .
2 . . * * * .
3 . . . . . *
4 . . . . . *

 typedef struct {
   size_t start;
   size_t end;
 } pair;


 Example invalid matrices:

  0 1 2 3 4 5
0 * * * . * * <-- more than one continuous run
1 . * * * . .
2 . * * * . .
3 . . * * * *
4 . . * * * *

  0 1 2 3 4 5
0 * * * . . .
1 . * * * . .
2 . * * * . .
3 * * * * * * <-- start index not monotonically increasing
4 . . * * * *


  0 1 2 3 4 5
0 * * * . . .
1 . * * * * .
2 . * * * . . <-- end index not monotonically increasing
3 . . * * * *
4 . . * * * *


-----------

 Let's examine the first case:

  0 1 2 3 4 5
0 * * * . . . <-- start at col 0, end at col 2
1 . * * * . . <-- start at col 1, end at col 3
2 . * * * . . <-- start at col 1, end at col 3
3 . . * * * *
4 . . * * * *

*/

#endif

typedef struct strided_mask_s {
    size_t n_rows;
    size_t n_cols;
    size_t* start_cols;
    size_t* end_cols;
} strided_mask_t;

/*
 * TODO: revisit if it makes sense to have separate arrays for start and end cols
 * I could see it being more cache-coherent to pack them one-after-another in a single array;
 * When we want a start index, we always want the end index, and having it right next to the start in memory
 * will nearly guarantee they're in the same cache line.
 * BENCHMARK later.
 */



/**
 * Creates a new empty strided_mask object with n_rows and n_cols.
 * Allocates memory, caller must handle cleanup.
 * @param n_rows
 * @param n_cols
 * @return An unpopulated strided mask.
 */
strided_mask_t* strided_mask_create(const size_t n_rows, const size_t n_cols);

/**
 * Creates a strided mask object from a list of start, end column indices
 * @param n_rows
 * @param n_cols
 * First n_rows var-args are start_cols, next n_rows var-args are end_cols
 * .return
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
 * A path_mask is a special kind of strided mask where the rows overlap by no more than one star,
 * and the upper left and lower right corner are filled.
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
 *
 * This function converts a (presumed) valid path mask to a sequence of index pairs
 * Also sets the value of path length to the number of points in the path.
 * Caller is responsible for cleaning up the memory returned.
 */
size_t* strided_mask_to_index_pairs(const strided_mask_t* mask, size_t* path_length);

size_t strided_mask_path_length(const strided_mask_t* mask);

/**
 * Build a strided mask object (a path mask) from a path.
 * Assumption is that path goes from upper left to lower right corner.
 * @param index_pairs
 * @param path_length
 * @return
 */
strided_mask_t* strided_mask_from_index_pairs(const size_t* index_pairs, const size_t path_length);

/**
 *
 * Upsamples the mask by a factor of two, then dilates the new cells by `radius` in each direction.
 * Returns a new mask; caller responsible for cleanup.
 * Example of r=1 dilation can be found at
 * https://www.dropbox.com/s/c8tqctwxyi80uub/Screenshot%202016-12-13%2015.40.53.png?dl=0
 * Worked example:
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
