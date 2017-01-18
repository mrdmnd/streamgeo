#ifndef UTILC_H
#define UTILC_H

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define TIMEIT(a)                                        \
do {                                                     \
    clock_t start=clock(), diff;                         \
    a;                                                   \
    diff = clock() - start;                              \
    printf("Time taken: %d ms\n", diff * 1000 / CLOCKS_PER_SEC); \
} while(0)

#endif
