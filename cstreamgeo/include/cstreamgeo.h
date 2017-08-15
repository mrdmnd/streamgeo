#ifndef CSTREAMGEO_H
#define CSTREAMGEO_H

#include <stddef.h>
#include <stdint.h>

/* ---------------- Core data structure types ---------------- */

typedef enum {POSITION, TIME, ELEVATION} stream_type_t;

typedef struct {
    int32_t lat;         // 10^6 * underlying float data (i.e. -38.591592 ==> -38591592)
    int32_t lng;         // 10^6 * underlying float data
} point_t;

typedef int32_t timestamp_t;   // Seconds past the unix epoch (or before, whatever)

typedef int32_t elevation_t;   // Meters above the reference geoid surface (or below, whatever)

/**
 * Some time series of data
 */
typedef struct {
    point_t* points;     // Stream data
    size_t n;            // Number of points in the stream.
} position_stream_t;

typedef struct {
    timestamp_t* timestamps;
    size_t n;
} timestamp_stream_t;

typedef struct {
    elevation_t* elevations;
    size_t n;
} elevation_stream_t;

typedef struct {
    stream_type_t stream_type;
    union {
        position_stream_t position_stream;
        timestamp_stream_t timestamp_stream;
        elevation_stream_t elevation_stream;
    } stream_contents;
} stream_t;

/**
 * A "heterogenous" collection of streams - these are linked together in index space, and correspond to a single trajectory.
 */
typedef struct {
    int64_t identifier;                    // Global identifier for this stream set (i.e. activity_id, or something)
    position_stream_t position_stream;     // NULL if no position information present.
    timestamp_stream_t timestamp_stream;   // NULL if no time information present.
    elevation_stream_t elevation_stream;   // NULL if no elevation information present.
} stream_set_t;


typedef struct {
    size_t* index_pairs; // Indices: [row_0, col_0, row_1, col_1, ..., row_N-1, col_N-1]
    size_t path_length;  // Number of *POINTS* in the warp path. The number of elts in `index_pairs` is 2x this value.
    int32_t cost;        // Result of aligning two streams.
} warp_summary_t;


/* ---------------- Stream Utility Functions ---------------- */


/**
 * Initializes storage for a stream with n points.
 * @param stream A pointer to an uninitialized stream object.
 * @param n Number of points in the stream
 * @param stream_type Type of datum.
 */
void stream_init(stream_t* stream, const size_t n, const stream_type_t stream_type);

/**
 * Initializes storage for a stream from a list of data.
 * @param stream A pointer to an uninitialized stream object.
 * @param n Number of points in the stream
 * @param stream_type Enum representing the data type.
 * @param ... List of data
 */
void stream_init_from_list(stream_t* stream, const size_t n, const stream_type_t stream_type, ...);

/**
 * Frees the memory allocated by `stream`.
 * @param stream
 */
void stream_release(const stream_t* stream);

/**
 * Print the (entire) contents of `stream`.
 * @param stream
 */
void stream_printf(const stream_t* stream);

/**
 * Print the contents of the stream, formatted as a GEOJSON feature.
 * @param stream
 */
void stream_geojson_printf(const position_stream_t* stream);

/**
 * Prints the number of points, distance, and sparsity (see docs) for the stream.
 * @param stream
 */
void stream_statistics_printf(const position_stream_t* stream);


/* ---------------- Stream Processing Routines ---------------- */


/**
 * Returns the length of the stream in its natural coordinate system.
 * For points in lat/lng encoding, this is the length of a stream in natural degrees (note: not meters)
 * @param stream
 */
double stream_distance(const position_stream_t* stream);

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
 * @param sparsity_out
 */
void stream_sparsity_init(const position_stream_t* stream, float* sparsity_out);

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
 * @param a First input stream
 * @param b Second input stream
 * @param warp_summary_out A warp_summary object containing the warp path, number of points in the warp path, and cost of alignment.
 */
void full_warp_summary_create(const stream_t* a, const stream_t* b, warp_summary_t* warp_summary_out);

/**
 * Returns the (approximately) optimal alignment of stream a to stream b.
 * Allocates memory; caller is responsible for cleanup.
 * Uses a fast dynamic timewarping algorithm by S. Salvador.
 * Larger radius values are increasingly more precise, but slower.
 * radius=0 and radius=1 are quite inaccurate;
 * radius=8 is O(3%) error on random (correlated) streams of size 1000 or so.
 * @param a First input stream
 * @param b Second input stream
 * @param radius Radius value
 * @param warp_summary_out A warp_summary object containing the warp path, number of points in the warp path, and cost of alignment.
 */
void fast_warp_summary_create(const stream_t* a, const stream_t* b, const size_t radius, warp_summary_t* warp_summary_out);

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
float similarity(const stream_t* a, const stream_t* b, const size_t radius);

void warp_summary_release(const warp_summary_t* warp_summary);

/* ---------------- Stream Resampling Routines ---------------- */

/**
 * Downsamples a stream intelligently with O(n log n) Ramer-Douglas-Peucker simplification.
 * @param input Input stream
 * @param epsilon Threshold for keeping points (points kept if distance exceeds epsilon)
 */
void downsample_rdp(position_stream_t* input, const float epsilon);

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
// size_t medoid_consensus(const stream_collection_t* input, const int approximate);

/**
 * Allocates space for, constructs, and returns a pointer to a synthetic "optimal element" for a
 * @param input Pointer to a stream collection
 * @param approxmiate Flag to use fast_dtw instead of full_dtw.
 *        If "approx" flag is set, computes alignment with radius set to ceil(max(stream_length)^(0.25))
 * @return A stream_t object that represents a (newly synthesized) "most representative" element from the stream collection.
 */
// stream_t* dba_consensus(const stream_collection_t* input, const int approximate, const size_t iterations);

#endif
