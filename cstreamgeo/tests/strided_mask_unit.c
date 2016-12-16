#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstreamgeo/stridedmask.h>

#include "test.h"

void create_test() {
    strided_mask_t* mask1 = strided_mask_create(2, 3);
    assert_non_null(mask1);
    strided_mask_destroy(mask1);

    /*
     *   0 1 2 3 4 5
     * 0 * * * . . .
     * 1 . * * * . .
     * 2 . * * * . .
     * 3 . . * * * *
     * 4 . . * * * *
     */
    strided_mask_t* mask2 = strided_mask_create_from_list(5, 6,   0, 1, 1, 2, 2,  2, 3, 3, 5, 5);
    strided_mask_printf(mask2);
    strided_mask_destroy(mask2);

    /*
     *   0 1 2 3 4 5
     * 0 * . . . . .
     * 1 * * . . . .
     * 2 . * . . . .
     * 3 . * * * . .
     * 4 . . . . * *
     */
    strided_mask_t* mask3 = strided_mask_create_from_list(5, 6,  0, 0, 1, 1, 4,  0, 1, 1, 3, 5);
    strided_mask_printf(mask3);
    strided_mask_destroy(mask3);
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
    strided_mask_t* mask = strided_mask_create_from_list(5, 6,  0, 0, 1, 1, 4,  0, 1, 1, 3, 5);
    strided_mask_printf(mask);



    size_t* path_length =  malloc(sizeof(size_t));
    const size_t* warp_path = strided_mask_to_index_pairs(mask, path_length);
    printf("Warp path has length %zu: [", *path_length);
    for (size_t i = 0; i < *path_length*2; i+=2) {
       printf("(%zu,%zu), ", warp_path[i], warp_path[i+1]);
    }
    printf("]\n");
    free(path_length);
    free((void*) warp_path);
    strided_mask_destroy(mask);
}

void mask_from_index_pairs_test() {
    /*
     *   0 1 2 3 4 5
     * 0 * . . . . .
     * 1 * * . . . .
     * 2 . * . . . .
     * 3 . * * * . .
     * 4 . . . . * *
     */
    const size_t path_length = 9;
    size_t* index_pairs = malloc(2*path_length*sizeof(size_t));
    index_pairs[0] = 0; index_pairs[1] = 0;
    index_pairs[2] = 1; index_pairs[3] = 0;
    index_pairs[4] = 1; index_pairs[5] = 1;
    index_pairs[6] = 2; index_pairs[7] = 1;
    index_pairs[8] = 3; index_pairs[9] = 1;
    index_pairs[10] = 3; index_pairs[11] = 2;
    index_pairs[12] = 3; index_pairs[13] = 3;
    index_pairs[14] = 4; index_pairs[15] = 4;
    index_pairs[16] = 4; index_pairs[17] = 5;

    strided_mask_t* mask = strided_mask_from_index_pairs(index_pairs, path_length);
    free(index_pairs);
    strided_mask_printf(mask);
    strided_mask_destroy(mask);
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
     * Output:
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
    strided_mask_t* mask = strided_mask_create_from_list(5, 6,  1, 1, 1, 2, 2,  2, 3, 3, 3, 4);
    strided_mask_printf(mask);
    strided_mask_t* expanded = strided_mask_expand(mask, 0);
    strided_mask_printf(expanded);
    strided_mask_destroy(mask);
    strided_mask_destroy(expanded);
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
     * Output:
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
    strided_mask_t* mask = strided_mask_create_from_list(5, 6,  1, 1, 1, 2, 2,  2, 3, 3, 3, 4);
    strided_mask_printf(mask);
    strided_mask_t* expanded = strided_mask_expand(mask, 1);
    strided_mask_printf(expanded);
    strided_mask_destroy(mask);
    strided_mask_destroy(expanded);

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
     * Output:
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
    strided_mask_t* mask = strided_mask_create_from_list(5, 6,  1, 1, 1, 2, 2,  2, 3, 3, 3, 4);
    strided_mask_printf(mask);
    strided_mask_t* expanded = strided_mask_expand(mask, 2);
    strided_mask_printf(expanded);
    strided_mask_destroy(mask);
    strided_mask_destroy(expanded);
}

int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(create_test),
            cmocka_unit_test(mask_from_index_pairs_test),
            cmocka_unit_test(mask_to_index_pairs_test),
            cmocka_unit_test(expand_radius_zero_test),
            cmocka_unit_test(expand_radius_one_test),
            cmocka_unit_test(expand_radius_two_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

