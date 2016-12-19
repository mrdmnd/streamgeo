#ifndef IO_H
#define IO_H

#include <cstreamgeo/cstreamgeo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Basic IO utilities for reading streams from files.
 * A collection of streams can be read from a file listing pairs of lat/lng points in JSON format, one per line.
 * e.g. the first line of the file is
 *   [[1.1,2.2],[3.3,4.4]]
 * and all the other lines represent other streams as well.
 * Streams can also be serialized and deserialized to a file in a binary format.
 */

typedef struct streams_s {
    stream_t** data;
    size_t n;
} streams_t;

/**
 * Load a collection of streams from the file specified by filename.
 * Each line of the file is assumed to represent one stream.
 * Each line (stream) should be a list of lat/long points, represented as a list with two elements
 */
streams_t* read_streams_from_json(const char* filename);

/**
 * Write the given collection of streams to the given file.
 * The struct is dumped in a binary format recursively.
 * If the file cannot be opened, this will fail silently.
 */
void write_streams_to_binary(const char* filename, const streams_t* streams);

/**
 * Read a collection of streams from the given file.
 * There is absolutely no error checking on the supplied file,
 * which may in some cases lead to a buffer overflow.
 */
streams_t* read_streams_from_binary(const char* filename);

#endif