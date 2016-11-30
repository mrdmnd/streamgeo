#include <stream.h>
/**
 * General functions for interacting with stream buffer data.
 */



/**
 * Returns the length of a stream in degrees (note: not meters)
 * @param s_n Number of points in the stream
 * @param s Stream data buffer
 * @return Length of the stream in degrees
 */
float stream_distance(const int s_n, const float* s) {
    assert(s_n >= 2);
    float sum = 0.0;
    for(int n = 0; n < s_n-1; n++) {
        float lat_diff = s[2*(n+1)] - s[2*n];
        float lng_diff = s[2*(n+1)+1] - s[2*n+1];
        sum += sqrt((lng_diff * lng_diff) + (lat_diff * lat_diff));
    }
    return sum;
}


/**
 * For each point in the stream buffer, compute the summed distance from that point to both neighbors --
 * the first and last point in the buffer count their neighbor twice.
 * Take the summed distance and normalize by 2 * optimal_spacing - the distance that each point would be from each
 * other point if the stream were perfectly spaced.
 * This procedure assigns a value [0, \infty) to each point.
 * Convert this domain into the range [0, 1] by passing the values x through 1- 2*atan(x)/pi.
 * @param s_n Number of points in the stream
 * @param s Stream buffer data
 * @param sparsity Output parameter for sparsity data. Assumed to have s_n points.
 */
void stream_sparsity(const int s_n, const float* s, float* sparsity) {
    float optimal_spacing = stream_distance(s_n, s) / (s_n-1);
    const float two_over_pi = 2.0 / M_PI;
    for(int n = 0; n < s_n; n++) {
        int i = (n - 1) < 0 ? 1 : n - 1;
        int j = n;
        int k = (n + 1) > s_n - 1 ? s_n - 2 : n + 1;
        float lat_diff_1 = s[2*j] - s[2*i];
        float lng_diff_1 = s[2*j + 1] - s[2*i + 1];
        float d1 = sqrt((lng_diff_1 * lng_diff_1) + (lat_diff_1 * lat_diff_1));
        float lat_diff_2 = s[2*k] - s[2*j];
        float lng_diff_2 = s[2*k + 1] - s[2*j + 1];
        float d2 = sqrt((lng_diff_2 * lng_diff_2) + (lat_diff_2 * lat_diff_2));
        float v = (d1 + d2) / (2 * optimal_spacing);
        sparsity[j] = 1.0 - two_over_pi * atan(v);
    }
}
