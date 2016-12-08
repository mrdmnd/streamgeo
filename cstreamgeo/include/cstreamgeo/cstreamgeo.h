#ifndef CSTREAMGEO_H
#define CSTREAMGEO_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <cstreamgeo/cstreamgeo_version.h>

/**
 * Basic geographic stream data structure.
 * Core assumption: streams are immutable - once data is set, it will not be changed.
 * You cannot "add points" to a stream; you must create a new stream with the points added.
 */
typedef struct stream_s {
    size_t n;
    float* data;
} stream_t;

/**
 * Creates a new stream (initially empty)
 */
stream_t* stream_create(void);

/**
 * Create a new stream (point array initialized to zeroes) of set capacity
 */
stream_t* stream_create_with_capacity(const size_t cap);

/**
 * Describe the summary structure of the stream (n_points, first point, last point)
 */
void stream_printf_summary(const stream_t* s);

/**
 * Print the (entire) contents of the stream.
 */
void stream_printf(const stream_t* s);

/**
 * Create a stream from a list of floats. This does memory allocation. Caller is responsible for management.
 */
stream_t* stream_from_list(const size_t n, ...);

/**
 * Copies a stream. This does memory allocation. Caller is responsible for management.
 */
stream_t* stream_copy(const stream_t* s);

/**
 * Frees the memory.
 */
void stream_free(const stream_t* s);

/**
 * Write the stream to an output pointer. Output buffer should have enough space allocated. 
 * Returns the number of bytes that were written.
 */
size_t stream_serialize(const stream_t* s, char* buffer);

/**
 * Read the input buffer to a stream_t object. This does memory allocation. Caller is responsible for management.
 */
stream_t* stream_deserialize(const void* buffer);

// Utility Functions

/**
 * Returns the length of the stream in its natural coordinate system.
 */
float stream_length(const stream_t* s);




