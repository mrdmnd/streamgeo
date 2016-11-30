#include <filters.h>

/**
 * Returns the index i in [start, end) such that the i'th point in input is the point that
 * minimizes the sum of the squared distance to all other points in input[start:end]
 * @param start Start index
 * @param end End index
 * @param input Data buffer
 * @return Index of the distance-sum-minimizing point.
 */
int geometric_medoid(const int start, const int end, const float *input) {
    int best_index = 0;
    float minimal_sum = FLT_MAX;
    for (int i = start; i < end; i++) {
        float i_lat = input[2*i];
        float i_lng = input[2*i+1];
        float i_sum = 0;
        for (int j = start; j < end; j++) {
            float lat_diff = input[2*j] - i_lat;
            float lng_diff = input[2*j+1] - i_lng;
            i_sum += (lat_diff*lat_diff) + (lng_diff*lng_diff);
        }
        if (i_sum < minimal_sum) {
            best_index = i;
            minimal_sum = i_sum;
        }
    }
    return best_index;
}

/**
 * Filters a data buffer to remove "outliers" by replacing each value with the median of a window centered on it
 * @param input_n Number of points in input buffer
 * @param input Data buffer
 * @param window Window width
 * @param output Output buffer with input_n points that contains filtered data.
 */
void median_filter(const int input_n, const float* input, const int window, float* output) {
    // Thinking about padding: we'll only median-filter from 0+half_window to input_n-half_window
    assert(window % 2 == 1);
    int half_window = (window - 1) / 2;
    for (int i = 0; i < half_window; i++) {
        output[2*i] = input[2*i];
        output[2*i+1] = input[2*i+1];
    }
    for (int i = input_n - half_window; i < input_n; i++) {
        output[2*i] = input[2*i];
        output[2*i+1] = input[2*i+1];
    }
    for (int i = half_window; i < input_n-half_window; i++) {
        int start = i - half_window;
        int end = i + half_window;
        int arg_medoid = geometric_medoid(start, end+1, input);
        output[2*i] = input[2*arg_medoid];
        output[2*i+1] = input[2*arg_medoid+1];
    }
}

/**
 * Computes the distance from a point to a line (defined by a start and end point), in two dimensions.
 * @param px Test point x
 * @param py Test point y
 * @param sx Start point x
 * @param sy Start point y
 * @param ex End point x
 * @param ey End point y
 * @return Distance between point p and line (s, e)
 */
float _point_line_distance(float px, float py, float sx, float sy, float ex, float ey) {
    if (sx == ex && sy == ey) {
        return (px-sx)*(px-sx) + (py-sy)*(py-sy);
    } else {
        return fabsf((ex-sx)*(sy-py) - (sx-px)*(ey-sy)) / sqrt((ex-sx)*(ex-sx) + (ey-sy)*(ey-sy));
    }
}

/**
 * Performs polyline simplification and identifies the indices to keep from a data buffer of points.
 * @param input_n Number of input points
 * @param input Input data buffer
 * @param start Start index to search from
 * @param end End index to stop searching at
 * @param epsilon Threshold for keeping points (keep if they exceed epsilon distance from line segment bound)
 * @param indices Output object containing indices of keep points.
 */
void _douglas_peucker(const int input_n, const float* input, const int start, const int end, const float epsilon, roaring_bitmap_t* indices) {
    float dmax = 0.0f; int index = start;
    float sx = input[2*start]; float sy = input[2*start+1];
    float ex = input[2*end]; float ey = input[2*end+1];
    for (int i = start+1; i < end; ++i) {
        float d = _point_line_distance(input[2*i], input[2*i+1], sx, sy, ex, ey);
        if (d > dmax) {
            index = i;
            dmax = d;
        }
    }
    if (dmax > epsilon) {
        if (index - start > 1) {
            _douglas_peucker(input_n, input, start, index, epsilon, indices);
        }
        roaring_bitmap_add(indices, index);
        if (end - index > 1) {
            _douglas_peucker(input_n, input, index, end, epsilon, indices);
        }
    }
}

/**
 * Wrapper around _douglas_peucker
 * @param input_n Number of input points
 * @param input Input data buffer
 * @param epsilon Threshold for keeping points (keep if they exceed epsilon distance)
 * @param output_n Number of points in output
 * @param output Output data buffer
 */
void reduce_by_rdp(const int input_n, const float* input, const float epsilon, int* output_n, float* output) {
    roaring_bitmap_t* indices = roaring_bitmap_create();
    roaring_bitmap_add(indices, 0);
    _douglas_peucker(input_n, input, 0, input_n-1, epsilon, indices);
    roaring_bitmap_add(indices, input_n-1);
    int n = 0;
    for (int i = 0; i < input_n; i++) {
        if (roaring_bitmap_contains(indices, i)) {
            output[2*n] = input[2*i];
            output[2*n+1] = input[2*i+1];
            n++;
        }
    }
    *output_n = n;
    roaring_bitmap_free(indices);
}
