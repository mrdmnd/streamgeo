# streamgeo
A space- and time-optimized collection of useful geometry algorithms
operating over geographic streams (polylines, trajectories, etc) and
collections of geographic streams.

This library is primarily implemented as a native C library for
performance reasons, but provides extensions for python3+ as well
as the JVM (Scala, Java, etc).

The project exists as a hobby/side project of mine, and is heavily
inspired by Daniel Lemire's work. It is not production-ready.

### Features

* Trajectory pre/post processing:
  For a given stream, it might be necessary to down- or up-sample the
  number of points. This library provides methods for simplifying
  (down-sampling) and interpolating (up-sampling) a stream.
  - Ramer-Douglas-Peucker decimation ( O(n log n), high quality )
  - Radial Distance decimation ( O(n), lower quality )
  - Adjacent-pairs averaging ( O(n), fixed downsampling )
  - Remove spiky data with Median filters of various widths
  - Savitzky Golay quadratic filters for smoothing

* Consensus Algorithms
  - For a collection of trajectories, which trajectory is "most representative?"
    - "Median" consensus mode chooses a pre-existing element of the collection:
       the trajectory that minimizes sum of distance to all other trajectories.
    - "DBA" consensus mode implements the Dynamic Barycenter Averaging algorithm of
      [F. Petitjean](http://dpt-info.u-strasbg.fr/~fpetitjean/Research/Petitjean2011-PR.pdf)

* Polyline Similarity Metrics
  - Dynamic Time Warp similarity ( O(n^2) )
  - Fast Approximate Dynamic Time Warp similarity  ( O(n) )
  - Hausdorff Distance ( O(n) )
  - Frechet Distance

* Working in geohash-space
  - Encode trajectories into sequences of length-K Geohash strings. (TODO)
  - Decode sequences of length-K geohash strings into trajectories. (TODO)
  - Identify a set of covering Geohashes for a stream at an arbitrary precision. (TODO)


* Other tools
  - Stream Sparsity (for each point in the stream, assign a value [0.0, 1.0]
    that represents the distance that point lies from its adjacent
    neighbors, normalized by the ideal average gap)
  - Stream Length (how long is a stream in degree-units?)

* A non-zero amount of tests! Hey! Cool!

* Performance benchmarks!

## Background
Streamgeo was initially developed for use in Data Science work at Strava.

I work with lots of "streams" of geographic data - the primary stream types are
  - "Segments" (short polylines on which athletes compete to cover in the shortest time)
    - Segments can include an altitude stream providing height above sea level in meters.
      Future versions of this library might provide mechanisms for interacting with these 3d streams.
      For now, Streamgeo works with the lat/lng data stream only.
    - Segments are "time-invariant" - they only include spatial coordinates.

  - "Activities" (long polylines which athletes record and upload with a GPS-enabled device).
    - Activities include a "time stamp" stream matching each index with a "seconds from activity start" value.
    - Activities can also include an altitude stream, like segments.


Fundamentally, these are both examples of GPS trajectories, and differ mainly in
  a) whether or not they include time-stamp data
  b) the number of points they contain (activities have roughly an order of magnitude more points)

Future versions of this library will provide mechanisms for interacting with time series and altitude data.
The initial focus, however, is on planar lat/lng stream data.

Initially, the primary storage for our streams is GZIPped JSON living on S3.

A sample stream for a segment might look like:

    8109834.json.gz

    [[37.395614,-122.247693],[37.395519,-122.24781],...,[37.372528,-122.253365],[37.372513,-122.253285]]

## Dependencies
The only third party dependency for Streamgeo is a fast, modern, cache-efficient implementation of sparse bitmasks: RoaringBitmap's CRoaring library

## Install

## Docker Image:
Streamgeo is available as a docker image. To build the image, run

    `docker build -t mredmond/streamgeo:v1 .`

To see inside the image, run

    `docker run -it mredmond/streamgeo:v1 /bin/bash`

### Install CRoaring:

CRoaring is organized as a CMake project.

    `git clone git@github.com:RoaringBitmap/CRoaring.git && cd CRoaring/`
    `mkdir build && cd build/`
    `cmake .. && make -j && sudo make install`

To get the linker to update its cache, on Ubuntu you might need to run
    `sudo ldconfig`

### Install gtk3

On fedora, I needed 

    `sudo dnf install gtk3-devel`

On ubuntu, I needed 

    `sudo apt-get install libgtk-3-dev`
### Install streamgeo

Streamgeo is also organized as a CMake project.

From the top level, create a build directory:
    `mkdir build && cd build/`

Next, switch to that directory, and
    `cmake .. && make && make install`

This will build (and install, if wanted) the C Native library first, then the python module pystreamgeo, and finally the JVM bindings (TODO).

## Usage:

    >>> import pystreamgeo.streamgeo as sg
    >>> s1 = [1.0, 1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 3.0, 4.0, 3.0, 5.0, 2.0, 6.0, 4.0, 4.0, 4.0]
    >>> s2 = [2.0, 1.0, 1.0, 1.0, 2.0, 3.0, 3.0, 3.0, 4.0, 4.0, 3.0, 4.0]

    Distance:
    >>> sg.stream_distance(s1)
    10.064495086669922

    Ramer Douglas Peucker simplification:
    >>> sg.reduce_by_rdp(s1, 2)
    [1.0, 1.0, 4.0, 3.0, 5.0, 2.0, 6.0, 4.0, 4.0, 4.0]

    Sparsity measure (0-1):
    >>> sg.stream_sparsity(s1)
    [0.613120436668396, 0.5553836226463318, 0.5553836226463318, 0.613120436668396, 0.5553836226463318, 0.42477625608444214, 0.37966489791870117, 0.3968008756637573]

    Fast DTW Alignment (with r=1):
    >>> sg.align(s1, s2, 1)
    (13.0, [0, 0, 1, 1, 2, 2, 3, 3, 4, 3, 5, 3, 6, 4, 7, 5])

    Similarity Metric (with r=1):
    >>> sg.similarity(s1, s2, 1)
    0.9646323323249817

## Benchmark and unit tests:
Core library unit tests and benchmarks are built as stand-alone executables in build/cstreamgeo.

    `cd build/cstreamgeo`
    `./medianfilter_test`
    `./medianfilter_bench`
    `./rdp_test`
    `./rdp_bench`
    `./similarity_test`
    `./similarity_bench`
    `./stream_test`


## Serialization and Loading Data
Our data is generally stored as lat/lng points with six figures of precision after the decimal point -
this is enough to get within 11cm of a target point, and reasonably represents the limits of GPS device precision.

In the C libraries, stream data is stored as a row-major one-dimensional array of floats:
our Old La Honda example is laid out like:

    `float old_la_honda[XXX] == {37.395614,-122.247693,37.395519,-122.24781,...,37.372528,-122.253365,37.372513,-122.253285};`

Even indices are the latitude coordinate and their value ranges between (-90.0, 90.0].
Odd indices are the longitude coordinate, and their value ranges between (-180.0, 180.0].

All functions that operate on streams expect to be passed arrays in these formats.

TODO: support loading google polyline encoded streams.

## Assumptions and Explicit Design Decisions
`streamgeo` is an opinionated library - raw performance optimization for
the common case is preferred almost exclusively over other considerations.
Wherever possible, I've made design choices that are efficient for our primary use case,
at the potential cost of speed/generality/abstraction for other use cases.

### Data
In general, all of our data is projected with EPSG 4326.

Almost all streams in our dataset are less than 300km long.
  - Segment streams are generally between 0km and 30km long.
  - Activity streams are generally between 0km and 200km long.

GPS sampling devices tend to have a maximum sampling resolution of about one point per second.
This means that:
  - Segment streams generally have between 500 and 3,000 points.
  - Activity streams generally have between 5000 and 30,000 points

`streamgeo` is tuned to work well with collections on the order of `O(10,000)` streams with these properties.

If you intend to use the library for very large streams, or very large collections of medium-length streams,
you may wish to spend some time thinking about parallelism or other optimization tricks.

The C library code is as memory-conservative as it can be, but our current focus is on single-threaded mode -- 
preliminary benchmarks show that the point at which parallelization performance crosses over serial performance
lies to the right of `n_points > 15,000`, which is generally larger than our largest streams.

### Distance metrics and projections
Because our trajectories are generally "short" and do not span much of the earth,
I make the "planar earth" approximation for distance computation.
This is done to minimize the number of trig calls we would have to do for
distance math, as these are generally slow. Frequently, you don't need the actual
distance between objects, but the relative distance - a convex metric such as
"squared euclidean distance" is sufficient.

The fundamental unit in `streamgeo` is not the meter, but the degree.
At the equator (lat = 0), one degree of longitude is approximately 111.321 km.
At the poles (lat = 90.0), one degree of longitude is zero meters. Womp womp.

We assume the earth is spherical (though it really is ellipsoidal) so
one degree of latitude is functionally 111km everywhere.


