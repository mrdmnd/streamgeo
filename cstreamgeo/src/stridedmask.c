#include <cstreamgeo/dbg.h>
#include <cstreamgeo/stridedmask.h>
#include <cstreamgeo/utilc.h>

strided_mask_t* strided_mask_create(const size_t n_rows, const size_t n_cols) {
    // NOTE: CALLER MUST SET first_start_col and first_end_col before using mask meaningfully.
    strided_mask_t* mask = malloc(sizeof(strided_mask_t));
    mask->n_rows = n_rows;
    mask->n_cols = n_cols;
    mask->start_deltas = malloc(n_rows * sizeof(uint8_t));
    mask->end_deltas = malloc(n_rows * sizeof(uint8_t));
    return mask;

}

strided_mask_t* strided_mask_create_from_list(const size_t n_rows, const size_t n_cols,
                                              const size_t first_start_col, const size_t first_end_col, ...) {
    va_list args;
    va_start(args, first_end_col);
    strided_mask_t* mask = strided_mask_create(n_rows, n_cols);
    mask->first_start_col = first_start_col;
    mask->first_end_col = first_end_col;
    for (size_t i = 0; i < n_rows; i++) {
        mask->start_deltas[i] = va_arg(args, int); // not strictly right; we want uint8_t's here, but compiler whines.
    }
    for (size_t i = 0; i < n_rows; i++) {
        mask->end_deltas[i] = va_arg(args, int);
    }
    va_end(args);
    return mask;

}

void strided_mask_destroy(const strided_mask_t* mask) {
    free(mask->start_deltas);
    free(mask->end_deltas);
    free((void*) mask);
}

void strided_mask_printf(const strided_mask_t* mask) {
    size_t n_rows = mask->n_rows;
    size_t n_cols = mask->n_cols;
    size_t start = mask->first_start_col;
    size_t end = mask->first_end_col;
    uint8_t* start_deltas = mask->start_deltas;
    uint8_t* end_deltas = mask->end_deltas;

    for (size_t row = 0; row < n_rows; row++) {
        start += start_deltas[row];
        end += end_deltas[row];
        for (size_t col = 0; col < n_cols; col++) {
            (start <= col && col <= end) ? printf("* ") : printf(". ");
        }
        printf("\n");
    }
}

size_t* strided_mask_to_index_pairs(const strided_mask_t* mask, size_t* path_length) {
    size_t n_rows = mask->n_rows;
    size_t n_cols = mask->n_cols;
    size_t start = mask->first_start_col;
    size_t end = mask->first_end_col;
    uint8_t* start_deltas = mask->start_deltas;
    uint8_t* end_deltas = mask->end_deltas;


    // Maximum size is n_rows + n_cols - 1 (follow the border, but don't double count corner)
    size_t* path = calloc(2 * (n_rows + n_cols - 1), sizeof(size_t));
    size_t index = 0;
    for (size_t row = 0; row < n_rows; row++) {
        start += start_deltas[row];
        end += end_deltas[row];
        for (size_t col = start; col <= end; col++) {
            path[index++] = row;
            path[index++] = col;
        }
    }
    *path_length = index / 2;
    return path;
}

void _expand_strided_mask(strided_mask_t* mask, const size_t radius) {
    size_t n_rows_initial = mask->n_rows;
    size_t n_cols_initial = mask->n_cols;
    size_t first_start_col_initial = mask->first_start_col;
    size_t first_end_col_initial = mask->first_end_col;
    uint8_t* start_deltas_initial = mask->start_deltas;
    uint8_t* end_deltas_initial = mask->end_deltas;

    size_t n_rows_upsampled = 2 * n_rows_initial;
    size_t n_cols_upsampled = 2 * n_cols_initial;
    size_t first_start_col_upsampled = 2*first_start_col_initial;
    size_t first_end_col_upsampled = 2*first_end_col_initial + 1;
    uint8_t* start_deltas_upsampled = malloc(n_rows_upsampled * sizeof(uint8_t));
    uint8_t* end_deltas_upsampled = malloc(n_rows_upsampled * sizeof(uint8_t));

    // First, compute and populate up-sampled mask directly:
    // TODO: change this to run from row = 0 to n_rows_final? maybe benchmark
    for (size_t row = 0; row < n_rows_initial; row++) {
        start_deltas_upsampled[2*row] = 2*start_deltas_initial[row];
        start_deltas_upsampled[2*row+1] = 0;
        end_deltas_upsampled[2*row] = 2*end_deltas_initial[row];
        end_deltas_upsampled[2*row+1] = 0;
    }

    // Decompress delta encoding for upsampled:
    // TODO: can this be done in delta-encoded space? that'd be freakin' magical
    size_t* start_cols_upsampled = malloc(n_rows_upsampled * sizeof(uint8_t));
    size_t* end_cols_upsampled = malloc(n_rows_upsampled * sizeof(uint8_t));
    size_t start = first_start_col_upsampled;
    size_t end = first_start_col_upsampled;
    for (size_t row = 0; row < n_rows_upsampled; row++) {
        start += start_deltas_upsampled[row];
        start_cols_upsampled[row] = start;

        end += end_deltas_upsampled[row];
        end_cols_upsampled[row] = start;
    }

    for (size_t row = 0; row < n_rows_upsampled; row++) {
        size_t start_ind_back_row = start_cols_upsampled[MAX(row-radius, 0)];
        size_t end_ind_forward_row = end_cols_upsampled[MIN(row + radius, n_rows_upsampled)];

    }

}