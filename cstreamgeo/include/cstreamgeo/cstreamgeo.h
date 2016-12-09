#ifndef CSTREAMGEO_H
#define CSTREAMGEO_H

#include <math.h>
#include <float.h>

#include <cstreamgeo/utilc.h>
#include <cstreamgeo/roaringmask.h>

/**
 * Basic geographic stream data structure.
 * Core assumption: streams are immutable - once data is set, it will not be changed.
 * You cannot "add points" to a stream; you must create a new stream with the points added.
 */
typedef struct stream_s {
    size_t n; // Number of *POINTS* in the stream. The number of floats in the data member is 2x this value.
    float* data;
} stream_t;


/* -------------- Utility Functions --------------- */

/**
 * Creates a new stream with n points.
 */
stream_t* stream_create(const size_t n);

/**
 * Create a stream from a list of floats. Number of POINTs is equal to n, but list must have 2n elements. 
 * Allocates memory; caller is responsible for cleanup.
 */
stream_t* stream_create_from_list(const size_t n, ...);

/**
 * Frees the memory allocated by `s`.
 */
void stream_destroy(const stream_t* stream);

/**
 * Print the (entire) contents of the stream.
 */
void stream_printf(const stream_t* stream);

/**
 * Prints the number of points, distance, and sparsity for the stream.
 */
void stream_printf_statistics(const stream_t* stream);

/**
 * Copies a stream. 
 * Allocates memory; caller is responsible for cleanup.
 */
stream_t* stream_copy(const stream_t* stream); // TODO

/**
 * Write the stream to an output pointer. Output buffer should have enough space allocated. 
 * Returns the number of bytes that were written.
 */
size_t stream_serialize(const stream_t* stream, char* buffer); // TODO

/**
 * Read the input buffer to a stream_t object. 
 * This does memory allocation. Caller is responsible for management.
 */
stream_t* stream_deserialize(const void* buffer); // TODO




/* -------------- Core Library Functions --------------- */

/**
 * Returns the length of the stream in its natural coordinate system.
 */
float stream_distance(const stream_t* stream);

/**
 * Returns an array with "sparsity" values for each point in the stream.
 * Allocates memory; caller is responsible for cleanup.
 */
float* stream_sparsity(const stream_t* stream);

/**
 * Returns the optimal alignment of stream a to stream b.
 * Uses the full O(M*N) dynamic timewarping algorithm.
 * Sets the output parameter `cost` equal to the alignment cost.
 * Allocates memory; caller is responsible for cleanup.
 */
size_t* full_align(const stream_t* a, const stream_t* b, float* cost, size_t* path_length); // TODO

/**
 * Returns the (approximately) optimal alignment of stream a to stream b.
 * Uses a fast dynamic timewarping algorithm by S. Salvador.
 * Larger radius values are increasingly more precise, but slower.
 * Allocates memory; caller is responsible for cleanup.
 */
size_t*
fast_align(const stream_t* a, const stream_t* b, const size_t radius, float* cost, size_t* path_length);  // TODO

/**
 * Establishes a "common-sense" distance metric on two streams.
 * Larger values for radius lead to slower code, but more accurate DTW alignment.
 * Short circuits on common cases.
 */
float redmond_similarity(const stream_t* a, const stream_t* b, const size_t radius); // TODO


/* -------------- Up/downsampling routines ----------------- */

/**
 * Downsamples a stream intelligently with O(n log n) Ramer-Douglas-Peucker simplification.
 * Allocates memory.
 */
stream_t* downsample_ramer_douglas_peucker(const stream_t* input, const float epsilon);

/**
 * Downsamples a stream quickly with O(n) radial-distance simplification.
 * Allocates memory.
 */
stream_t* downsample_radial_distance(const stream_t* input, const float epsilon); // TODO

/**
 * Downsamples a stream *very* quickly (O(n), but with very good constant) by selecting all points at index zero mod `factor`.
 * Allocates memory.
 */
stream_t* downsample_fixed_factor(const stream_t* input, const size_t factor); // TODO

/**
 * Upsamples a stream by a fixed factor. Fills in points by linear interpolation.
 * Allocates memory.
 */
stream_t* upsample_fixed_factor(const stream_t* input, const size_t factor); // TODO

/*
 * Resamples a stream by the rational fraction M / N by upsampling at factor M and downsamping at factor N.
 */
stream_t* resample_fixed_factor(const stream_t* input, const size_t m, const size_t n);  // TODO

#endif
