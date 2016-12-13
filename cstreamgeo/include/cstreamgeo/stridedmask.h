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
1 . * * * . . <-- start moves up 1, end moves up 1
2 . * * * . . <-- start moves up 0, end moves up 0
3 . . * * * *
4 . . * * * *

 Store the first start, end indices: (0, 2): because these could be large, we want these to be size_t's.
 Then for each row (including the obvious (0, 0) for the first row), store the increment of the start and end indices:
 start_deltas = (0, 1, 0, 1, 0)
 end_deltas = (0, 1, 0, 2, 0)

 The size of the each deltas array is n_rows.

 TODO:
 These values are *very* small, and we want to save space.
 Consider a "small integer" storage scheme rather than use uint8_t's? Serialization will require looping or CPU cleverness.
 Maybe the compiler can do this intelligently?
 0 = "00"
 1 = "01"
 2 = "100"
 3 = "101"
 4 = "1100"
 5 = "1101"
 6 = "11100"
 7 = "11101"
 8 = "111100"
 9 = "111101"
10 = "1111100"
11 = "1111101"
*/

#endif

typedef struct strided_mask_s {
    size_t n_rows;
    size_t n_cols;
    size_t first_start_col;
    size_t first_end_col;
    uint8_t* start_deltas;
    uint8_t* end_deltas;
} strided_mask_t;

/*
 * TODO: revisit if it makes sense to have separate arrays for start and end deltas.
 * I could see it being more cache-coherent to pack them one-after-another in a single array;
 * When we want a start index, we always want the end index, and having it right next to the start in memory
 * will nearly guarantee they're in the same cache line.
 * BENCHMARK later.
 */



/**
 * Creates a new empty strided_mask object with n_rows and n_cols.
 * Explicitly does *NOT* set first_start_col and first_end_col, caller must set.
 * Allocates memory, caller must handle cleanup.
 * @param n_rows
 * @param n_cols
 * @return
 */
strided_mask_t* strided_mask_create(const size_t n_rows, const size_t n_cols);

/**
 * Creates a strided mask object from a list of deltas
 * @param n_rows
 * @param n_cols
 * @param first_start_col
 * @param first_end_col
 * First n_rows var-args are start_deltas, next r_rows var-args are end_deltas
 * @return
 */
strided_mask_t* strided_mask_create_from_list(const size_t n_rows, const size_t n_cols,
                                              const size_t first_start_col, const size_t first_end_col, ...);

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
 * A path_mask is a special kind of strided mask where the rows overlap by no more than one star
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
 * This function converts a valid path mask to a sequence of index pairs
 * Also sets the value of path length to the number of points in the path.
 * Caller is responsible for cleaning up the memory returned.
 */
size_t* strided_mask_to_index_pairs(const strided_mask_t* mask, size_t* path_length);
