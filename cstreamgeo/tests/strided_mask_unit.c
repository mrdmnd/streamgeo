#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstreamgeo/stridedmask.h>

#include "test.h"

void create_test() {
    strided_mask_t* mask1 = strided_mask_create(2, 3);
    mask1->first_start_col = 0;
    mask1->first_end_col = 1;
    assert_non_null(mask1);
    strided_mask_destroy(mask1);

    strided_mask_t* mask2 = strided_mask_create_from_list(5, 6,  0, 2,   0, 1, 0, 1, 0,  0, 1, 0, 2, 0);
    strided_mask_printf(mask2);

    /* 5 Rows, 6 Columsn.
     * First start, end indices: (0, 2).
     * The increment of the start and end indices for each row
     * (0, 0)
     * (1, 1)
     * (0, 0)
     * (1, 2)
     * (0, 0)
     *   0 1 2 3 4 5
     * 0 * * * . . . <-- start at col 0, end at col 2
     * 1 . * * * . . <-- start moves up 1, end moves up 1
     * 2 . * * * . . <-- start moves up 0, end moves up 0
     * 3 . . * * * *
     * 4 . . * * * *
     */
    strided_mask_destroy(mask2);


}

void index_pairs_test() {
    /*
     *   0 1 2 3 4 5
     * 0 * . . . . .
     * 1 * * . . . .
     * 2 . * . . . .
     * 3 . * * * . .
     * 4 . . . . * *
     */


    strided_mask_t* mask = strided_mask_create_from_list(5, 6,  0, 0,   0, 0, 1, 0, 3,  0, 1, 0, 2, 2);
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



int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(create_test),
            cmocka_unit_test(index_pairs_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

