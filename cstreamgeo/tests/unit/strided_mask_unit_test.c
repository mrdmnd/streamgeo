#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <stridedmask.h>

#include "test.h"

void create_test() {
    /* Empty (non-initialized data) mask
     * Just want to verify that the arrays are being allocated.
     */
    strided_mask_t mask1;
    strided_mask_init(&mask1, 2, 3);
    assert_non_null(mask1.start_cols);
    assert_non_null(mask1.end_cols);
    strided_mask_release(&mask1);

    /*
     *   0 1 2 3 4 5
     * 0 * * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * * *
     * 4 . . * * * *
     */
    strided_mask_t mask2;
    strided_mask_init_from_list(&mask2, 5, 6,   0, 1, 1, 2, 2,  2, 3, 3, 5, 5);
    strided_mask_printf(&mask2);
    strided_mask_release(&mask2);

    /*
     *   0 1 2 3 4 5
     * 0 * . . . . .
     * 1 * * . . . .
     * 2 . * . . . .
     * 3 . * * * . .
     * 4 . . . . * *
     */
    strided_mask_t mask3;
    strided_mask_init_from_list(&mask3, 5, 6,  0, 0, 1, 1, 4,  0, 1, 1, 3, 5);
    strided_mask_printf(&mask3);
    strided_mask_release(&mask3);
}

void mask_to_index_pairs_test() {
    /*
     *   0 1 2 3 4 5
     * 0 * . . . . .
     * 1 * * . . . .
     * 2 . * . . . .
     * 3 . * * * . .
     * 4 . . . . * *
     */
    strided_mask_t mask;
    strided_mask_init_from_list(&mask, 5, 6,  0, 0, 1, 1, 4,  0, 1, 1, 3, 5);
    strided_mask_printf(&mask);

    size_t* warp_path = malloc(2 * (mask.n_rows + mask.n_cols - 1) * sizeof(size_t));
    const size_t path_length = strided_mask_to_index_pairs(&mask, warp_path);

    printf("Warp path has length %zu: [", path_length);
    for (size_t i = 0; i < 2*path_length; i+=2) {
       printf("(%zu,%zu), ", warp_path[i], warp_path[i+1]);
    }
    printf("]\n");
    free((void*) warp_path);
    strided_mask_release(&mask);
}

void strided_expand_test_base(const size_t row_parity, const size_t col_parity, const size_t radius) {
    const int n_rows = 5;
    const int n_cols = 6;
    strided_mask_t mask;
    strided_mask_init_from_list(&mask, n_rows, n_cols,  1, 1, 1, 2, 2,  2, 3, 3, 3, 4);
    strided_mask_printf(&mask);

    strided_mask_t expanded;
    strided_mask_init(&expanded, 2*n_rows + row_parity, 2*n_cols + col_parity);
    strided_mask_expand(&mask, row_parity, col_parity, radius, &expanded);
    strided_mask_printf(&expanded);

    strided_mask_release(&mask);
    strided_mask_release(&expanded);
}

void expand_radius_zero_test() {
    // Expansion by radius zero still is an upscale by 2x
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     * Desired Output:
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
     */
    strided_expand_test_base(0, 0, 0);
}

void expand_radius_zero_row_parity_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     * Desired Output: (adds a row at the bottom)
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
     * 0 . . . . . . . . . . . .
     */
    strided_expand_test_base(1, 0, 0);
}

void expand_radius_zero_col_parity_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     * Desired Output:
     *   0 1 2 3 4 5 6 7 8 9 0 1 2 (adds a column at right)
     * 0 . . * * * * . . . . . . .
     * 1 . . * * * * . . . . . . .
     * 2 . . * * * * * * . . . . .
     * 3 . . * * * * * * . . . . .
     * 4 . . * * * * * * . . . . .
     * 5 . . * * * * * * . . . . .
     * 6 . . . . * * * * . . . . .
     * 7 . . . . * * * * . . . . .
     * 8 . . . . * * * * * * . . .
       9 . . . . * * * * * * . . .
     */
    strided_expand_test_base(0, 1, 0);
}

void expand_radius_zero_row_col_parity_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     * Desired Output: (add a row to bottom, col to right)
     *   0 1 2 3 4 5 6 7 8 9 0 1 2
     * 0 . . * * * * . . . . . . .
     * 1 . . * * * * . . . . . . .
     * 2 . . * * * * * * . . . . .
     * 3 . . * * * * * * . . . . .
     * 4 . . * * * * * * . . . . .
     * 5 . . * * * * * * . . . . .
     * 6 . . . . * * * * . . . . .
     * 7 . . . . * * * * . . . . .
     * 8 . . . . * * * * * * . . .
     * 9 . . . . * * * * * * . . .
     * 0 . . . . . . . . . . . . .
     */
    strided_expand_test_base(1, 1, 0);
}

void expand_radius_one_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     *
     * Desired Output:
     *   0 1 2 3 4 5 6 7 8 9 0 1
     * 0 . * * * * * * . . . . .
     * 1 . * * * * * * * * . . .
     * 2 . * * * * * * * * . . .
     * 3 . * * * * * * * * . . .
     * 4 . * * * * * * * * . . .
     * 5 . * * * * * * * * . . .
     * 6 . * * * * * * * * . . .
     * 7 . . . * * * * * * * * .
     * 8 . . . * * * * * * * * .
     * 9 . . . * * * * * * * * .
     */
    strided_expand_test_base(0, 0, 1);
}

void expand_radius_one_row_parity_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     *
     * Desired Output:
     *   0 1 2 3 4 5 6 7 8 9 0 1
     * 0 . * * * * * * . . . . .
     * 1 . * * * * * * * * . . .
     * 2 . * * * * * * * * . . .
     * 3 . * * * * * * * * . . .
     * 4 . * * * * * * * * . . .
     * 5 . * * * * * * * * . . .
     * 6 . * * * * * * * * . . .
     * 7 . . . * * * * * * * * .
     * 8 . . . * * * * * * * * .
     * 9 . . . * * * * * * * * .
     * 0 . . . * * * * * * * * .
     */
    strided_expand_test_base(1, 0, 1);
}

void expand_radius_one_col_parity_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     *
     * Desired Output:
     *   0 1 2 3 4 5 6 7 8 9 0 1 2
     * 0 . * * * * * * . . . . . .
     * 1 . * * * * * * * * . . . .
     * 2 . * * * * * * * * . . . .
     * 3 . * * * * * * * * . . . .
     * 4 . * * * * * * * * . . . .
     * 5 . * * * * * * * * . . . .
     * 6 . * * * * * * * * . . . .
     * 7 . . . * * * * * * * * . .
     * 8 . . . * * * * * * * * . .
     * 9 . . . * * * * * * * * . .
     */
    strided_expand_test_base(0, 1, 1);
}

void expand_radius_one_row_col_parity_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     *
     * Desired Output:
     *   0 1 2 3 4 5 6 7 8 9 0 1 2
     * 0 . * * * * * * . . . . . .
     * 1 . * * * * * * * * . . . .
     * 2 . * * * * * * * * . . . .
     * 3 . * * * * * * * * . . . .
     * 4 . * * * * * * * * . . . .
     * 5 . * * * * * * * * . . . .
     * 6 . * * * * * * * * . . . .
     * 7 . . . * * * * * * * * . .
     * 8 . . . * * * * * * * * . .
     * 9 . . . * * * * * * * * . .
     * 0 . . . * * * * * * * * . .
     */
    strided_expand_test_base(1, 1, 1);
}

void expand_radius_two_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     *
     * Desired Output:
     *   0 1 2 3 4 5 6 7 8 9 0 1
     * 0 * * * * * * * * * * . .
     * 1 * * * * * * * * * * . .
     * 2 * * * * * * * * * * . .
     * 3 * * * * * * * * * * . .
     * 4 * * * * * * * * * * . .
     * 5 * * * * * * * * * * . .
     * 6 * * * * * * * * * * * *
     * 7 * * * * * * * * * * * *
     * 8 . . * * * * * * * * * *
     * 9 . . * * * * * * * * * *
     */
    strided_expand_test_base(0, 0, 2);
}
void expand_radius_two_row_col_parity_test() {
    /* Input:
     *   0 1 2 3 4 5
     * 0 . * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * . .
     * 4 . . * * * .
     *
     * Desired Output:
     *   0 1 2 3 4 5 6 7 8 9 0 1 2
     * 0 * * * * * * * * * * . . .
     * 1 * * * * * * * * * * . . .
     * 2 * * * * * * * * * * . . .
     * 3 * * * * * * * * * * . . .
     * 4 * * * * * * * * * * . . .
     * 5 * * * * * * * * * * . . .
     * 6 * * * * * * * * * * * * .
     * 7 * * * * * * * * * * * * .
     * 8 . . * * * * * * * * * * .
     * 9 . . * * * * * * * * * * .
     * 0 . . * * * * * * * * * * .
     */
    strided_expand_test_base(1, 1, 2);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(create_test),
        cmocka_unit_test(mask_to_index_pairs_test),
        cmocka_unit_test(expand_radius_zero_test),
        cmocka_unit_test(expand_radius_zero_row_parity_test),
        cmocka_unit_test(expand_radius_zero_col_parity_test),
        cmocka_unit_test(expand_radius_zero_row_col_parity_test),
        cmocka_unit_test(expand_radius_one_test),
        cmocka_unit_test(expand_radius_one_row_parity_test),
        cmocka_unit_test(expand_radius_one_col_parity_test),
        cmocka_unit_test(expand_radius_one_row_col_parity_test),
        cmocka_unit_test(expand_radius_two_test),
        cmocka_unit_test(expand_radius_two_row_col_parity_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

