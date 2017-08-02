#include <cstreamgeo/cstreamgeo.h>
#include <cstreamgeo/io.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "test.h"
#include "config.h" // Contains make-time generated benchmark_data_dir #define macro



void io_test_collection_size_one() {
    char filename[1024];
    size_t bddl = strlen(BENCHMARK_DATA_DIR);
    strcpy(filename, BENCHMARK_DATA_DIR);
    strcpy(filename+bddl, "segments/8109834.json");

    const stream_collection_t* streams_from_json = read_streams_from_json(filename);
    if (!streams_from_json) {
        printf("Unable to load streams from file '%s'\n", filename);
        return;
    }

    const char* output = "output.stream";
    write_streams_to_binary(output, streams_from_json);
    stream_collection_destroy(streams_from_json);

    const stream_collection_t* streams_from_binary = read_streams_from_binary(output);
    stream_collection_printf(streams_from_binary);
    stream_collection_destroy(streams_from_binary);
    remove(output); // Delete output file.
}

void io_test_collection_size_many() {
    char filename[1024];
    size_t bddl = strlen(BENCHMARK_DATA_DIR);
    strcpy(filename, BENCHMARK_DATA_DIR);
    strcpy(filename+bddl, "segments/oldlahondas.json");

    const stream_collection_t* streams_from_json = read_streams_from_json(filename);
    if (!streams_from_json) {
        printf("Unable to load streams from file '%s'\n", filename);
        return;
    }

    stream_collection_printf(streams_from_json);
    stream_collection_destroy(streams_from_json);

}

int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(io_test_collection_size_one),
            cmocka_unit_test(io_test_collection_size_many),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
