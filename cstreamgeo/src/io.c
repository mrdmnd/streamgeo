#include <cstreamgeo/io.h>
#include <sys/types.h>

stream_t* _load_from_json_line(char* line) {
    stream_t* stream = malloc(sizeof(stream_t));
    
    size_t capacity = 128;
    stream->data = malloc(capacity * sizeof(float));
    stream->n = 0;  // During the construction of the stream, let n represent the actual number of floats.

    // Strip all characters in `[],` and tokenize the rest
    const char* delim = "[],";
    char *token;
    for (token = strtok(line, delim); token; token = strtok(NULL, delim)) {
        float num = strtof(token, NULL); // TODO: proper error handling
        
        // Grow if necessary.
        if (stream->n == capacity) {
            stream->data = realloc(stream->data, 2 * capacity * sizeof(float));
            capacity *= 2;
        }

        // Append the new element.
        stream->data[stream->n] = num;
        stream->n++;
    }

    stream->n /= 2;  // Update n for consistency with the rest of the project.
    return stream;
}

void _write_stream_fp(FILE* fp, const stream_t* stream) {
    fwrite(&(stream->n), sizeof(size_t), 1, fp);
    fwrite(stream->data, sizeof(float), stream->n * 2, fp);
}

stream_t* _read_stream_fp(FILE* fp) {
    // TODO We currently just dump whatever memory crap is in the file into the struct.
    // We really should make sure that the size indicators are valid (so we don't try
    // to malloc a huge number accidentally), and ensure that what we get back looks
    // reasonable.
    stream_t* stream = malloc(sizeof(stream_t));
    fread(&(stream->n), sizeof(size_t), 1, fp);
    
    stream->data = malloc(2 * stream->n * sizeof(float));  // Now that we know how big it is...
    fread(stream->data, sizeof(float), 2 * stream->n, fp);
    return stream;
}


/**
 * Top level functions exposed by the header
 */

streams_t* read_streams_from_json(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Unable to open file '%s' for reading.\n", filename);
        return NULL;
    }
    streams_t* streams = malloc(sizeof(streams_t));
    size_t capacity = 8;
    streams->data = malloc(capacity * sizeof(stream_t *));
    streams->n = 0;  // How many actual streams there are
    ssize_t bytes_read;
    char* line = NULL;
    size_t n = 0;
    while (getline(&line, &n , fp) != -1) {
        stream_t* stream = _load_from_json_line(line);
        if (streams->n == capacity) {
            streams->data = realloc(streams->data, 2 * capacity * sizeof(stream_t*));
            capacity *= 2;
        }
        streams->data[streams->n] = stream;
        streams->n++;
    }
    fclose(fp);
    return streams;
}

void write_streams_to_binary(const char* filename, const streams_t* streams) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Unable to open file '%s' for writing.\n", filename);
        return;  // TODO Fail more noisily
    }
    fwrite(&(streams->n), sizeof(size_t), 1, fp);
    for (int i = 0; i < streams->n; ++i) {
        _write_stream_fp(fp, streams->data[i]);
    }
    fclose(fp);
}

streams_t* read_streams_from_binary(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Unable to open file '%s' for reading.\n", filename);
        return NULL;
    }
    streams_t* streams = malloc(sizeof(streams_t));
    fread(&(streams->n), sizeof(size_t), 1, fp);
    streams->data = malloc(streams->n * sizeof(stream_t *));
    for (int i = 0; i < streams->n; i++) {
        streams->data[i] = _read_stream_fp(fp);
    }
    fclose(fp);
    return streams;
}