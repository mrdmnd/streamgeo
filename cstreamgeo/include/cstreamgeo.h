#ifndef CSTREAMGEO_H
#define CSTREAMGEO_H

#include <stddef.h>

/* ---------------- Core data structure types ---------------- */

// A "GEOHASH" is a 64
typedef int64_t point_t;
// 1  0  0  1  0  0  1  0  0  1  0  0  1  0  0  1  0  0  1  0  0  1  0  0  1  0  0  1  0  0  1  0 // latitude component are even-indexed bits
//  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1

typedef struct {
    int32_t* lats;       // Encoded as 10^6 * degree value (i.e. 90 degrees ==> 90000000)
    int32_t* lngs;
    size_t n;            // Number of points in the stream.
} stream_t;

typedef struct {
    stream_t** data;     // Buffer containing pointers to streams.
    size_t n;            // Number of streams in the collection
} stream_collection_t;

typedef struct {
    size_t* index_pairs; // Indices: [row_0, col_0, row_1, col_1, ..., row_N-1, col_N-1]
    size_t path_length;  // Number of *POINTS* in the warp path. The number of elts in `index_pairs` is 2x this value.
    int32_t cost;          // Result of aligning two streams.
} warp_summary_t;


/* ---------------- Stream Utility Functions ---------------- */

void distance

/**
 * Creates a new stream with n points.
 * Allocates memory; caller must clean up.
 * @param n Number of points in the stream
 * @return A pointer to a stream_t object.
 */
stream_t* stream_create(const size_t n);

/**
 * Create a stream from a list of floats. Number of POINTs is equal to n, but list must have 2n elements.
 * Allocates memory; caller is responsible for cleanup.
 * @param n Number of points in the stream
 * @param ... List of [lat0, lng0, lat1, lng1, ... latN-1, lngN-1] data
 * @return A pointer to a stream_t object.
 */
stream_t* stream_create_from_list(const size_t n, ...);

/**
 * Frees the memory allocated by `stream`.
 * @param stream
 */
void stream_destroy(const stream_t* stream);

/**
 * Print the (entire) contents of `stream`.
 * @param stream
 */
void stream_printf(const stream_t* stream);

/**
 * Print the contents of the stream, formatted as a GEOJSON feature.
 * @param stream
 */
void stream_geojson_printf(const stream_t* stream);

/**
 * Prints the number of points, distance, and sparsity (see docs) for the stream.
 * @param stream
 */
void stream_statistics_printf(const stream_t* stream);


/* ---------------- Stream Processing Routines ---------------- */


/**
 * Returns the length of the stream in its natural coordinate system.
 * For points in lat/lng encoding, this is the length of a stream in degrees (note: not meters)
 * @param stream
 */
float stream_distance(const stream_t* stream);

/**
 * Returns an array with "sparsity" values for each point in the stream.
 * Allocates memory for sparsity array; caller is responsible for cleanup.
 * For each point in the stream buffer, compute the summed distance from that point to both neighbors --
 * the first and last point in the buffer count their neighbor twice.
 * Take the summed distance and normalize by 2 * optimal_spacing - the distance that each point would be from each
 * other point if the stream were perfectly spaced.
 * This procedure assigns a value [0, \infty) to each point.
 * Convert this domain into the range [0, 1] by passing the values x through 1- 2*atan(x)/pi.
 * @param stream
 * @return Outputs sparsity data. Assumed to have s_n points. Allocates memory. Caller must clean up.
 */
float* stream_sparsity_create(const stream_t *stream);

/**
 * Returns the COST of the optimal alignment of stream `a` to stream `b`, but not the path.
 * Uses an approach that is O(M*N) in TIME but only O(max(M, N)) in space - we can get away
 * with storing only the previous columns and the current columns if we do not care about tracking
 * where we came from. In practice, this is 30% faster than computing the alignment.
 * @param a First input stream
 * @param b Second input stream
 * @return The cost of aligning the two streams
 */
int32_t full_dtw_cost(const stream_t* a, const stream_t* b);

/**
 * Returns the optimal alignment of stream `a` to stream `b`.
 * Uses the full O(M*N) dynamic timewarping algorithm.
 * Allocates memory for returned warp_summary object; caller is responsible for cleanup.
 * @param a First input stream
 * @param b Second input stream
 * @return A warp_summary object containing the warp path, number of points in the warp path, and cost of alignment.
 */
warp_summary_t* full_warp_summary_create(const stream_t *a, const stream_t *b);

/**
 * Returns the (approximately) optimal alignment of stream a to stream b.
 * Allocates memory; caller is responsible for cleanup.
 * Uses a fast dynamic timewarping algorithm by S. Salvador.
 * Larger radius values are increasingly more precise, but slower.
 * radius=0 and radius=1 are quite inaccurate;
 * radius=8 is O(3%) error on random (correlated) streams of size 1000 or so.
 * @param a First input stream
 * @param b Second input stream
 * @return A warp_summary object containing the warp path, number of points in the warp path, and cost of alignment.
 */
warp_summary_t* fast_warp_summary_create(const stream_t *a, const stream_t *b, const size_t radius);

/**
 * Establishes a "common-sense" distance metric on two streams.
 * Larger values for radius lead to slower code, but more accurate DTW alignment.
 * radius=0 and radius=1 are quite inaccurate;
 * radius=8 is O(3%) error on random (correlated) streams of size 1000 or so.
 * Short circuits on common cases.
 * @param a First input stream
 * @param b Second input stream
 * @return Value of similarity metric on the two input streams.
 */
float similarity(const stream_t *a, const stream_t *b, const size_t radius);

void warp_summary_destroy(const warp_summary_t* warp_summary);

/* ---------------- Stream Resampling Routines ---------------- */


/**
 * Downsamples a stream intelligently with O(n log n) Ramer-Douglas-Peucker simplification.
 * Allocates memory for simplified stream object; caller must clean up.
 * @param input Input stream
 * @param epsilon Threshold for keeping points (points kept if distance exceeds epsilon)
 */
void downsample_rdp(stream_t *input, const float epsilon);

/**
 * NOT YET IMPLEMENTED
 * Downsamples a stream quickly with O(n) radial-distance simplification.
 * @paramm input Stream to downsample
 * @param epsilon Threshold for keeping points (points kept if distance exceeds epsilon)
 * @return Downsampled stream
 */
// stream_t* downsample_radial_distance(const stream_t* input, const float epsilon);

/**
 * NOT YET IMPLEMENTED
 * Resamples a stream by the rational fraction M / N by upsampling at factor M and downsamping at factor N.
 * Allocates memory for simplified stream object; caller must clean up.
 * @param input Stream to resample by M/N
 * @param m Upsampling factor
 * @param n Downsampling factor
 * @return Resampled stream
 */
// stream_t* resample_fixed_factor(const stream_t* input, const size_t M, const size_t N);


/* ---------------- Operations on Stream Collections ---------------- */


/**
 * Computes the index of the "most median" element of a stream collection.
 * @param input Pointer to a stream collection
 * @param approximate Flag to use fast_dtw instead of full_dtw.
 *        If "approx" flag is set to 1, computes alignment with radius set to ceil(max(stream_length)^(0.25))
 *        Otherwise                   , computes full alignment.
 * @return An index into the stream_collection_t that selects the (previously existing) "most representative" element from the stream collection.
 */
size_t medoid_consensus(const stream_collection_t* input, const int approximate);

/**
 * Allocates space for, constructs, and returns a pointer to a synthetic "optimal element" for a
 * @param input Pointer to a stream collection
 * @param approxmiate Flag to use fast_dtw instead of full_dtw.
 *        If "approx" flag is set, computes alignment with radius set to ceil(max(stream_length)^(0.25))
 * @return A stream_t object that represents a (newly synthesized) "most representative" element from the stream collection.
 */
stream_t* dba_consensus(const stream_collection_t* input, const int approximate, const size_t iterations);

#endif
