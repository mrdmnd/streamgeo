#ifndef STREAM_H
#define STREAM_H
#include <assert.h>
#include <math.h>

float stream_distance(const int s_n, const float* s);
void stream_sparsity(const int s_n, const float* s, float* sparsity);

#endif // STREAM_H
