#ifndef FILTERS_H
#define FILTERS_H

#include <math.h>
#include <float.h>
#include <roaring.h>
#include <util.h>

void reduce_by_rdp(const int input_n, const float* input, const float epsilon, int* output_n, float* output);

int geometric_medoid(const int start, const int end, const float *input);
void median_filter(const int input_n, const float* input, const int window, float* output);

#endif // FILTERS_H
