#ifndef CSTREAMGEO_H
#define CSTREAMGEO_H

#include <cstreamgeo/version.h>
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
 * Creates a new stream with set capacity; all data is zeroed.
 */
stream_t* stream_create(const size_t cap);

/**
 * Create a stream from a list of floats. Number of POINTs is equal to n, but list must have 2n elements. 
 * Allocates memory; caller is responsible for cleanup.
 */
stream_t* stream_create_from_list(const size_t n, ...);

/**
 * Frees the memory allocated by `s`.
 */
void stream_destroy(const stream_t* s);

/**
 * Print the (entire) contents of the stream, prefixed with the number of points.
 */
void stream_printf(const stream_t* s);

/**
 * Copies a stream. 
 * Allocates memory; caller is responsible for cleanup.
 */
stream_t* stream_copy(const stream_t* s);

/**
 * Write the stream to an output pointer. Output buffer should have enough space allocated. 
 * Returns the number of bytes that were written.
 */
size_t stream_serialize(const stream_t* s, char* buffer);

/**
 * Read the input buffer to a stream_t object. 
 * This does memory allocation. Caller is responsible for management.
 */
stream_t* stream_deserialize(const void* buffer);




/* -------------- Core Library Functions --------------- */

/**
 * Returns the length of the stream in its natural coordinate system.
 */
float stream_length(const stream_t* s);

/**
 * Returns an array with "sparsity" values for each point in the stream.
 * Allocates memory; caller is responsible for cleanup.
 */
float* stream_sparsity(const stream_t* s);

/**
 * Returns the cost of (exactly) optimally aligning stream a to stream b.
 * Uses the full O(M*N) dynamic timewarping algorithm.
 */
float full_align(const stream_t* a, const stream_t* b, roaring_mask_t* alignment);

/**
 * Returns the cost of (approximately) optimally aligning stream a to stream b.
 * Uses a fast dynamic timewarping algorithm by S. Salvador.
 * Larger radius values are increasingly more precise, but slower.
 */
float fast_align(const stream_t* a, const stream_t* b, const size_t radius, roaring_mask_t* alignment);


/* -------------- Up/downsampling routines ----------------- */

/**
 * Downsamples a stream intelligently with O(n log n) Ramer-Douglas-Peucker simplification. 
 */
void downsample_ramer_douglas_peucker(const stream_t* input, const float epsilon, stream_t* output);

/**
 * Downsamples a stream quickly with O(n) radial-distance simplification.
 */
void downsample_radial_distance(const stream_t* input, const float epsilon, stream_t* output);

/**
 * Downsamples a stream *very* quickly (O(n), but with very good constant) by selecting all points at index zero mod `factor`.
 */
void downsample_fixed_factor(const stream_t* input, const size_t factor, stream_t* output);

/**
 * Upsamples a stream by a fixed factor. Fills in points by linear interpolation.
 */
void upsample_fixed_factor(const stream_t* input, const size_t factor, stream_t* output);

/*
 * Resamples a stream by the rational fraction M / N by upsampling at factor M and downsamping at factor N.
 */
void resample_fixed_factor(const stream_t* input, const size_t m, const size_t n, stream_t* output);

#endif
