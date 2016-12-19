#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstreamgeo/cstreamgeo.h>
#include <cstreamgeo/io.h>

#include "test.h"

void streams_printf(const streams_t* streams) {
    const size_t n = streams->n;
    const stream_t** data = (const stream_t**) streams->data;
    printf("Contains %zu streams: [\n", n);
    for (size_t i = 0; i < n; i++) {
        printf("  ");
        stream_printf(data[i]);
    }
    printf("]\n");
}

void stream_printf(const stream_t* stream) {
    const size_t n = stream->n;
    const float* data = stream->data;
    printf("Stream contains %zu points: [", n);
    for (size_t i = 0; i < n; i++) {
        printf("(%f %f), ", data[2 * i], data[2 * i + 1]);
    }
    printf("]\n");
}

void io_test_small() {
    const char *filename = "/home/mredmond/streamgeo/cstreamgeo/benchmarks/realdata/segments/8109834.json";

    const streams_t* streams_from_json = read_streams_from_json(filename);
    if (!streams_from_json) {
        printf("Unable to load streams from file '%s'\n", filename);
        return;
    }

    const char* output = "output.stream";
    write_streams_to_binary(output, streams_from_json);
    streams_destroy(streams_from_json);

    const streams_t* streams_from_binary = read_streams_from_binary(output);
    streams_printf(streams_from_binary);
    streams_destroy(streams_from_binary);
}

int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(io_test_small),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
