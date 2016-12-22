#ifndef IO_H
#define IO_H

#include <cstreamgeo/cstreamgeo.h>

/**
 * Allocate space for a new collection of streams.
 * @param n
 * @return A collection that can hold `n` stream_t* pointers.
 */
stream_collection_t* stream_collection_create(const size_t n);


/**
 * Destroy a streams collection object, and recursively destory all streams inside of it.
 * @param streams
 */
void stream_collection_destroy(const stream_collection_t* streams);

/**
 * Print a stream collection object.
 * @param streams
 */
void stream_collection_printf(const stream_collection_t* streams);


/**
 * Load a collection of streams from the file specified by `filename`.
 * Each line of the file is assumed to represent one stream.
 * Each line (stream) should be a list of lat/long points, represented as a list with two elements
 * @param filename Path to a file to load from
 * @return A pointer to a constant stream collection, containing one stream for each line in the file.
 */
const stream_collection_t* read_streams_from_json(const char* filename);


/**
 * NOT YET IMPLEMENTED
 * Write a collection of streams into the file at `filename`.
 * Format is identical to the corresponding `read` method.
 * @param filename Path to a file to write out.
 * @param streams A stream collection object.
 */
void write_streams_to_json(const char* filename, const stream_collection_t* streams);


/**
 * Read a collection of streams from the given file.
 * File format is custom binary.
 * @param filename Path to a file to load from
 * @return A pointer to a constant stream collection, containing one stream for each line in the file.
 */
const stream_collection_t* read_streams_from_binary(const char* filename);


/**
 * Write a collection of streams to the given file.
 * @param filename Path to a file to write out.
 * @param streams A stream collection object.
 */
void write_streams_to_binary(const char* filename, const stream_collection_t* streams);

#endif
