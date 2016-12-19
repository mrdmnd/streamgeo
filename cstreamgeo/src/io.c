#include "io.h"

streams_t* load_from_json_lines_fp(FILE* fp);
stream_t* load_from_json_line(char* line);

void write_streams_fp(FILE* fp, const streams_t* streams);
void write_stream_fp(FILE* fp, const stream_t* stream);

streams_t* read_streams_fp(FILE* fp);
stream_t* read_stream_fp(FILE* fp);

void streams_printf(const streams_t* streams);
void stream_printf(const stream_t* stream);

streams_t* load_from_json_lines(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Unable to open file '%s' for reading.\n", filename);
        return NULL;
    }

    streams_t* streams = load_from_json_lines_fp(fp);
    fclose(fp);
    return streams;
}

streams_t* load_from_json_lines_fp(FILE* fp) {
    streams_t* streams = malloc(sizeof(streams_t));

    // We'll start with a buffer and grow it when/if we need to
    size_t capacity = 10;
    streams->data = malloc(capacity * sizeof(stream_t *));
    streams->n = 0;  // How many actual streams there are

    ssize_t read;
    char *line = NULL;
    size_t unused_len = 0;
    while ((read = getline(&line, &unused_len, fp)) != -1) {
        stream_t* stream = load_from_json_line(line);

        // Grow if necessary
        if (streams->n == capacity) {
            streams->data = realloc(streams->data, 2 * capacity);
            capacity *= 2;
        }

        // Append the new element.
        streams->data[streams->n] = stream;
        streams->n++;
    }
    return streams;
}


stream_t* load_from_json_line(char* line) {
    stream_t* stream = malloc(sizeof(stream_t));
    
    size_t capacity = 10;
    stream->data = malloc(capacity * sizeof(float));
    stream->n = 0;  // During the construction of the stream, let n represent the actual number of floats.

    // Strip all characters in `[],` and tokenize the rest
    const char* delim = "[],";
    char *token;
    for (token = strtok(line, delim); token; token = strtok(NULL, delim)) {
        char* rest;
        float num = strtof(token, &rest);  // TODO: check `rest` to see if converted properly
        
        // Grow if necessary.
        if (stream->n == capacity) {
            stream->data = realloc(stream->data, 2 * capacity);
            capacity *= 2;
        }

        // Append the new element.
        stream->data[stream->n] = num;
        stream->n++;
    }

    stream->n /= 2;  // Update n for consistency with the rest of the project.
    return stream;
}


/* High-level utilities for reading/writing collections of streams to files */

void write_streams(const char* filename, const streams_t* streams) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Unable to open file '%s' for writing.\n", filename);
        return;  // TODO Fail more noisily
    }
    write_streams_fp(fp, streams);
    fclose(fp);
}

streams_t* read_streams(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Unable to open file '%s' for reading.\n", filename);
        return NULL;
    }
    streams_t* ret = read_streams_fp(fp);
    fclose(fp);
    return ret;
}

/* I/O at the level of collections of streams (streams_t) on open file pointers */

void write_streams_fp(FILE* fp, const streams_t* streams) {
    fwrite(&(streams->n), sizeof(size_t), /* nitems = */ 1, fp);
    for (int i = 0; i < streams->n; ++i) {
        write_stream_fp(fp, streams->data[i]);
    }
}

streams_t* read_streams_fp(FILE* fp) {
    // TODO We're still blindly trusting the contents of the file. Let's *not* do that
    streams_t* streams = malloc(sizeof(streams_t));
    fread(&(streams->n), sizeof(size_t), /* nitems = */ 1, fp);

    streams->data = malloc(streams->n * sizeof(stream_t *));
    for (int i = 0; i < streams->n; i++) {
        streams->data[i] = read_stream_fp(fp);
    }
    return streams;
}

/* I/O at the level of individual streams */

void write_stream_fp(FILE* fp, const stream_t* stream) {
    fwrite(&(stream->n), sizeof(size_t), /* nitems = */ 1, fp);
    fwrite(stream->data, sizeof(float),  /* nitems = */ stream->n * 2, fp);
}

stream_t* read_stream_fp(FILE* fp) {
    // TODO We currently just dump whatever memory crap is in the file into the struct.
    // We really should make sure that the size indicators are valid (so we don't try
    // to malloc a huge number accidentally), and ensure that what we get back looks
    // reasonable.
    stream_t* stream = malloc(sizeof(stream_t));
    fread(&(stream->n), /* size = */ sizeof(size_t), /* nitems = */ 1, fp);
    
    stream->data = malloc(2 * stream->n * sizeof(float));  // Now that we know how big it is...
    fread(stream->data, /* size = */ sizeof(float), /* nitems = */ 2 * stream->n, fp);
    return stream;
}
/**
 * TESTING
 */

int main() {
    const char *filename = "8109834.json";
    
    streams_t* streams = load_from_json_lines(filename);
    if (!streams) {
        printf("Unable to load streams from file '%s'\n", filename);
        return -1;
    }
    streams_printf(streams);

    // stream_t* stream = streams->data[0];
    // // streams_printf(streams);

    const char* output = "output.stream";
    write_streams(output, streams);

    streams = read_streams(output);
    streams_printf(streams);

    return 0;
}

/* Handy debugging functions */

void streams_printf(const streams_t* streams) {
    const size_t n = streams->n;
    const stream_t** data = streams->data;
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