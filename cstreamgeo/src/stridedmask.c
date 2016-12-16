#include <cstreamgeo/dbg.h>
#include <cstreamgeo/stridedmask.h>
#include <cstreamgeo/utilc.h>

strided_mask_t* strided_mask_create(const size_t n_rows, const size_t n_cols) {
    strided_mask_t* mask = malloc(sizeof(strided_mask_t));
    mask->n_rows = n_rows;
    mask->n_cols = n_cols;
    mask->start_cols = malloc(n_rows * sizeof(size_t));
    mask->end_cols = malloc(n_rows * sizeof(size_t));
    return mask;

}

strided_mask_t* strided_mask_create_from_list(const size_t n_rows, const size_t n_cols, ...) {
    va_list args;
    va_start(args, n_cols);
    strided_mask_t* mask = strided_mask_create(n_rows, n_cols);
    for (size_t i = 0; i < n_rows; i++) {
        mask->start_cols[i] = va_arg(args, int); // not strictly right; we want size_t's here, but the compiler is unhappy
    }
    for (size_t i = 0; i < n_rows; i++) {
        mask->end_cols[i] = va_arg(args, int);
    }
    va_end(args);
    return mask;

}

void strided_mask_destroy(const strided_mask_t* mask) {
    free(mask->start_cols);
    free(mask->end_cols);
    free((void*) mask);
}

void strided_mask_printf(const strided_mask_t* mask) {
    const size_t n_rows = mask->n_rows;
    const size_t n_cols = mask->n_cols;
    const size_t* start_cols = mask->start_cols;
    const size_t* end_cols = mask->end_cols;

    // Header
    printf("  ");
    for (size_t col = 0; col < n_cols; col++) {
        printf("%zu ", col % 10);
    }
    printf("\n");

    size_t start, end;
    for (size_t row = 0; row < n_rows; row++) {
        printf("%zu ", row % 10);
        start = start_cols[row];
        end = end_cols[row];
        for (size_t col = 0; col < n_cols; col++) {
            (start <= col && col <= end) ? printf("* ") : printf(". ");
        }
        printf("\n");
    }
}

size_t* strided_mask_to_index_pairs(const strided_mask_t* mask, size_t* path_length) {
    const size_t n_rows = mask->n_rows;
    const size_t n_cols = mask->n_cols;
    const size_t* start_cols = mask->start_cols;
    const size_t* end_cols = mask->end_cols;

    // Maximum size is n_rows + n_cols - 1 (follow the border, but don't double count corner)
    size_t* path = calloc(2 * (n_rows + n_cols - 1), sizeof(size_t));
    size_t index = 0;
    size_t start, end;
    for (size_t row = 0; row < n_rows; row++) {
        start = start_cols[row];
        end = end_cols[row];
        for (size_t col = start; col <= end; col++) {
            path[index++] = row;
            path[index++] = col;
        }
    }
    *path_length = index / 2;
    return path;
}

strided_mask_t* strided_mask_from_index_pairs(const size_t* index_pairs, const size_t path_length) {
    // [0, 0, 1, 0, 1, 1, 2, 1, 3, 1, 3, 2, 3, 3, 4, 4, 4, 5]
    //  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7
    // path_length = 9
    const size_t n_rows = index_pairs[2*(path_length-1)] + 1;
    const size_t n_cols = index_pairs[2*(path_length-1)+1] + 1;
    strided_mask_t* mask = strided_mask_create(n_rows, n_cols);
    size_t* start_cols = mask->start_cols;
    size_t* end_cols = mask->end_cols;

    int old_row = -1;
    int old_col = -1;
    int new_row;
    int new_col;
    // Boundary initializers
    start_cols[0] = 0;
    end_cols[n_rows] = n_cols;
    for (size_t index = 0; index < path_length; index++) {
        new_row = (int) index_pairs[2 * index];
        new_col = (int) index_pairs[2 * index + 1];
        if (new_row > old_row) {
            start_cols[new_row] = (size_t) new_col;
            end_cols[old_row] = (size_t) old_col;
        }
        old_row = new_row;
        old_col = new_col;
    }
    return mask;
}

strided_mask_t* strided_mask_expand(const strided_mask_t* mask, const size_t radius) {
    const size_t n_rows_initial = mask->n_rows;
    const size_t n_cols_initial = mask->n_cols;
    const size_t* start_cols_initial = mask->start_cols;
    const size_t* end_cols_initial = mask->end_cols;

    const size_t n_rows_final = 2 * n_rows_initial;
    const size_t n_cols_final = 2 * n_cols_initial;
    size_t* start_cols_final = malloc(n_rows_final * sizeof(size_t));
    size_t* end_cols_final = malloc(n_rows_final * sizeof(size_t));

    for (int row = 0; row < (int) n_rows_final; row++) { // NOTE: this is an int on purpose, we want to be able to subtract values and see negative numbers.
        start_cols_final[row] = (size_t) MAX((int) (2 * start_cols_initial[MAX(row - (int) radius,                  0) / 2]    ) - (int) radius,                  0);
        end_cols_final[row]   = (size_t) MIN((int) (2 *   end_cols_initial[MIN(row + (int) radius, (int) n_rows_final) / 2] + 1) + (int) radius, (int) n_rows_final);
    }

    strided_mask_t* retmask = malloc(sizeof(strided_mask_t));
    retmask->n_rows = n_rows_final;
    retmask->n_cols = n_cols_final;
    retmask->start_cols = start_cols_final;
    retmask->end_cols = end_cols_final;
    return retmask;
}