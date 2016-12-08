#ifndef ROARINGMASK_H
#define ROARINGMASK_H
#include <roaring.h>
#include <stdio.h>
#include <dbg.h>

/* Example masks:
 *
 * 0 0
 * 0 1
 * 1 0
 *
 * Is represented by (3, 2, roaring([3, 4]))
 *
 * 1 0 0 0
 * 0 1 0 0
 * 0 0 1 0
 * 0 0 0 1
 *
 * Is represented by (4, 4, roaring([0, 5, 10, 15]))
 *
 * Masks are used internally to represent both DTW Warp Paths and DTW Windows in a space-efficient manner
 */
typedef struct {
    int n_rows;
    int n_cols;
    roaring_bitmap_t* indices;
} RoaringMask;

RoaringMask* roaring_mask_create(int n_rows, int n_cols, int initial_capacity);
void roaring_mask_destroy(RoaringMask* m);
void roaring_mask_print(RoaringMask m);

#endif // ROARINGMASK_H
