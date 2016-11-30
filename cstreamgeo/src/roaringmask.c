#include <roaringmask.h>

/**
 * Initialization for a RoaringMask object
 * @param n_rows Number of rows
 * @param n_cols Number of columns
 * @param initial_capacity Pre-allocates space for indexes, because we store non-zero entries only.
 * @return RoaringMask object
 */
RoaringMask* roaring_mask_create(int n_rows, int n_cols, int initial_capacity) {
    RoaringMask* m = malloc(sizeof(RoaringMask));
    if (m == NULL) {
        log_err("Could not allocate memory for mask");
        return NULL;
    }
    m->n_rows = n_rows;
    m->n_cols = n_cols;
    m->indices = roaring_bitmap_create_with_capacity(initial_capacity);
    if (m->indices == NULL) {
        log_err("Could not allocate memory for mask indices.");
        roaring_mask_destroy(m);
        return NULL;
    }
    return m;
}

/**
 * Roaring mask destructor
 * @param m Mask to destroy
 */
void roaring_mask_destroy(RoaringMask* m) {
    if (m) {
        roaring_bitmap_free(m->indices);
        free(m);
    } else {
        log_warn("Attempted to free a null mask pointer.");
    }
}

/**
 * Debug tool for visualization of a RoaringMask object
 * @param m Mask to vizualize
 */
void roaring_mask_print(RoaringMask m) {
    for(int i = 0; i < m.n_rows; i++) {
        for(int j = 0; j < m.n_cols; j++) {
            roaring_bitmap_contains(m.indices, i * m.n_cols + j) ? printf("1 ") : printf("0 ");
        }
        printf("\n");
    }
}
