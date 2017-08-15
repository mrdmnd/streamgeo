#include <stridedmask.h>
#include <utilc.h>
#include <stdio.h>

void strided_mask_init(strided_mask_t* mask, const size_t n_rows, const size_t n_cols) {
    mask->n_rows = n_rows;
    mask->n_cols = n_cols;
    mask->start_cols = malloc(n_rows * sizeof(size_t));
    mask->end_cols = malloc(n_rows * sizeof(size_t));
}

void strided_mask_init_from_list(strided_mask_t* mask, const size_t n_rows, const size_t n_cols, ...) {
    va_list args;
    va_start(args, n_cols);
    strided_mask_init(mask, n_rows, n_cols);
    for (size_t i = 0; i < n_rows; i++) {
        mask->start_cols[i] = va_arg(args, int); // not strictly right; we want size_t's here, but the compiler is unhappy
    }
    for (size_t i = 0; i < n_rows; i++) {
        mask->end_cols[i] = va_arg(args, int);
    }
    va_end(args);
}

void strided_mask_release(const strided_mask_t* mask) {
    free(mask->start_cols);
    free(mask->end_cols);
}

void strided_mask_printf(const strided_mask_t* mask) {
    const size_t* start_cols = mask->start_cols;
    const size_t* end_cols = mask->end_cols;
    printf("  ");
    for (size_t col = 0; col < mask->n_cols; col++) {
        printf("%zu ", col % 10);
    }
    printf("\n");
    size_t start, end;
    for (size_t row = 0; row < mask->n_rows; row++) {
        printf("%zu ", row % 10);
        start = start_cols[row];
        end = end_cols[row];
        for (size_t col = 0; col < mask->n_cols; col++) {
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
    size_t* path = malloc(2 * (n_rows + n_cols - 1) * sizeof(size_t));
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
    path = realloc(path, index * sizeof(size_t));
    *path_length = index / 2;
    return path;
}

// The dimensions of mask_out must be 2*n_rows_initial + row_parity x 2*n_rows_initial + col_parity
// mask_out must be pre-init'd from above
void strided_mask_expand(const strided_mask_t* mask_in, const int row_parity, const int col_parity, const size_t radius, strided_mask_t* mask_out) {
    const size_t n_rows_initial = mask_in->n_rows;
    const size_t n_cols_initial = mask_in->n_cols;
    const size_t n_rows_final = 2 * n_rows_initial + row_parity;
    const size_t n_cols_final = 2 * n_cols_initial + col_parity;

    for (int row = 0; row < (int) n_rows_final; row++) {
        // NOTE: `row` is an int on purpose, we want to be able to subtract values and see negative numbers.
        size_t prev_row = (size_t) (MIN ( MAX(row - (int) radius,                          0), 2*((int) n_rows_initial-1)) / 2);
        size_t next_row = (size_t) (MAX ( MIN(row + (int) radius, 2*((int) n_rows_initial-1)),                          0) / 2);
        mask_out->start_cols[row] = (size_t) MAX((int) (2 * mask_in->start_cols[prev_row]    ) - (int) radius,                                 0);
        mask_out->end_cols[row]   = (size_t) MIN((int) (2 *   mask_in->end_cols[next_row] + 1) + (int) radius + col_parity, (int) n_cols_final-1);
    }
}
