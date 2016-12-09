#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstreamgeo/roaringmask.h>

#include "test.h"

void create_test() {
    roaring_mask_t* mask1 = roaring_mask_create(3, 2, 2);
    assert_non_null(mask1);
    roaring_mask_destroy(mask1);

    roaring_mask_t* mask2 = roaring_mask_create_from_list(3, 2, 2,   3, 4);
    roaring_mask_printf(mask2);
    /*
     * 0 0
     * 0 1
     * 1 0
     *
     * Is represented by (3, 2, roaring([3, 4]))
     */
     roaring_mask_destroy(mask2);
}

void index_pairs_test() {
    assert_int_equal(1, 1);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(create_test),
        cmocka_unit_test(index_pairs_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}