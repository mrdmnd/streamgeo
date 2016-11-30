#ifndef SIMILARITY_H
#define SIMILARITY_H

#include <stdlib.h>
#include <float.h>
#include <stdio.h>
#include <math.h>
#include <filters.h>
#include <stream.h>
#include <roaringmask.h>
#include <util.h>

float align(const int a_n, const float* a, const int b_n, const float* b, const int radius, int* path_length, int* path_xy);
float similarity(const int a_n, const float* a, const int b_n, const float* b, const int radius);

#endif // SIMILARITY_H
