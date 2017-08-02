[![Build Status](https://travis-ci.org/mrdmnd/streamgeo.svg?branch=master)](https://travis-ci.org/mrdmnd/streamgeo)

# streamgeo

`Streamgeo` is an aggressively space- and runtime- optimized collection of
tempospatial geometry algorithms operating over time-series data linked to
geographic streams (polylines, trajectories, etc) and collections of geographic streams.

This library is implemented as a native C library, but also provides
bindings for the JVM (see `jstreamgeo` directory).

## Dependencies

There are no external dependencies for `streamgeo.`

## Installation

`Streamgeo` is organized like a typical CMake project. Building "out of
source" is recommended.

From the top level, create a build directory:
    `mkdir build && cd build/`

Next, switch to that directory, and
    `cmake .. && make -j && make test`

Unit tests live in `build/tests/xxx_test` and benchmarks live in
`build/benchmarks/xxx_benchmark`

To install the library on your machine, run
    `make install`

This will build (and install, if wanted) the C Native library first, then the python module pystreamgeo, and finally the JVM bindings (TODO).

## Features

Currently, `streamgeo` supports the following feature set:

* Stream pre/post processing:
  For a given stream, it might be necessary to down- or up- sample the
  number of points. This library provides methods for simplifying
  (down-sampling) and interpolating (up-sampling) a stream.
  - Ramer-Douglas-Peucker simplification ( O(n log n), high quality )
  - Radial Distance simplification ( O(n), lower quality ) (TODO)
  - Median-filtering to remove "spikey" data. (TODO)
  - Savitzky Golay quadratic filters for smoothing (TODO)

* Consensus Algorithms
  - For a collection of trajectories, which trajectory is "most representative?"
    - "Median" consensus mode chooses a pre-existing element of the collection:
       the trajectory that minimizes sum of distance to all other trajectories. (TODO)
    - "DBA" consensus mode implements the Dynamic Barycenter Averaging algorithm of
      [F. Petitjean](http://dpt-info.u-strasbg.fr/~fpetitjean/Research/Petitjean2011-PR.pdf) (TODO)

* Polyline Similarity Metrics
  - Dynamic Time Warp similarity ( O(n^2) )
  - Fast Approximate Dynamic Time Warp similarity  ( O(n) )
  - Hausdorff Distance ( O(n) ) (TODO)
  - Frechet Distance (TODO)

* Clustering
  - HDBSCAN Clustering on collections of streams (TODO)

* Serialization / Deserialization
  - Read/write functions from/to GeoJSON
  - Read/write functions from/to custom binary serialization as buffer of floats.
  - Read/write functions from/to Length-K Geohash strings. (TODO)
  - Read/write functions from/to Google Polyline Encoding format (TODO)

* Other tools
  - Stream Sparsity (for each point in the stream, assign a value [0.0, 1.0]
    that represents the distance that point lies from its adjacent
    neighbors, normalized by the ideal average gap)
  - Stream Length (how long is a stream in degree-units?)

* A non-zero amount of tests! Neat-o!

* Performance benchmarks! SO FAST!

### Distance metrics and projections

Because we assume input trajectories are generally "short" and do not
span much of the earth, `streamgeo` makes the "planar earth"
approximation for distance computation: distance between points is not
computed with the haversine formula, but rather the standard "euclidean
distance" squared in degree-space. This is done to minimize the number
of trigonometric function invocations calls we would have to do for more
accurate distance math, as these are generally slow. Indeed, for many of
our intended / support use cases, callers don't need the actual distance
between objects, and an approximation is acceptable. In general, for most
applications, a log-convex metric such as "squared euclidean distance" is sufficient.

The fundamental unit in `streamgeo` is not the meter, but the degree.
At the equator (lat = 0), one degree of longitude is approximately 111.321 km.
At the poles (lat = 90.0), one degree of longitude is zero meters.

We assume the earth is spherical (though it really is ellipsoidal) so
one degree of latitude is functionally 111km everywhere.

## Storage Formats / IO / Serialization
At Strava, the primary source-of-truth storage format for our streams are
JSON files. Our data is stored as a sequence of floating point lat/lng
values with six figures of precision after the decimal point - this is
enough to get within 11cm of a target point, and reasonably represents
the limits of GPS device precision.


A sample stream for a segment might look like:

    $ cat 8109834.json

    [[37.395614,-122.247693],[37.395519,-122.24781],...,[37.372528,-122.253365],[37.372513,-122.253285]]

`Streamgeo` can read this format, and also provides an efficient binary
representation for inter-process serialization: a buffer of floats,
prepended with an integer containing the number of points in the stream.

In the C libraries, stream buffer data is stored as a row-major one-dimensional array of floats.
Our Old La Honda example is laid out like:

    `float old_la_honda[XXX] == {37.395614,-122.247693,37.395519,-122.24781,...,37.372528,-122.253365,37.372513,-122.253285};`

Even indices are the latitude coordinate and their value ranges between (-90.0, 90.0].
Odd indices are the longitude coordinate, and their value ranges between (-180.0, 180.0].

