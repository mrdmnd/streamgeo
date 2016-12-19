#include <cstreamgeo/io.h>
#include <sys/types.h>

streams_t* streams_create(const size_t n) {
    streams_t* streams = malloc(sizeof(streams_t));
    streams->n = n;
    streams->data = malloc(n * sizeof(stream_t*));
    return streams;
}

void streams_destroy(const streams_t* streams) {
    for (size_t i = 0; i < streams->n; i++) {
        stream_destroy(streams->data[i]);
    }
    free(streams->data);
    free((void*) streams);

}

stream_t* _load_from_json_line(char* line) {
    const char* delim = "[],";
    stream_t* stream = malloc(sizeof(stream_t));
    size_t capacity = 128;
    size_t n_tokens = 0;
    float* data = malloc(capacity * sizeof(float));
    char *token;
    for (token = strtok(line, delim); token; token = strtok(NULL, delim)) {
        float num = strtof(token, NULL);
        if (n_tokens == capacity) {
            data = realloc(data, 2 * capacity * sizeof(float));
            capacity *= 2;
        }
        data[n_tokens] = num;
        n_tokens++;
    }
    stream->data = data;
    stream->n = n_tokens / 2;
    return stream;
}

void _write_stream_fp(FILE* fp, const stream_t* stream) {
    fwrite(&(stream->n), sizeof(size_t), 1, fp);
    fwrite(stream->data, sizeof(float), stream->n * 2, fp);
}

stream_t* _read_stream_fp(FILE* fp) {
    stream_t* stream = malloc(sizeof(stream_t));
    fread(&(stream->n), sizeof(size_t), 1, fp);
    
    stream->data = malloc(2 * stream->n * sizeof(float));  // Now that we know how big it is...
    fread(stream->data, sizeof(float), 2 * stream->n, fp);
    return stream;
}


/**
 * Top level functions exposed by the header
 */

const streams_t* read_streams_from_json(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Unable to open file '%s' for reading.\n", filename);
        return NULL;
    }

    streams_t* streams = malloc(sizeof(streams_t));
    size_t capacity = 8;
    size_t n_streams = 0;
    stream_t** data = malloc(capacity * sizeof(stream_t *));

    char* line;
    size_t line_size = 512; // unused, but a hint to the getline fn
    size_t characters; // also unused

    line = malloc(line_size * sizeof(char));
    if (line == NULL) {
        perror("Unable to allocate buffer for getline()");
    }

    while ((characters = getline(&line, &line_size , fp)) != -1) {
        stream_t* stream = _load_from_json_line(line);
        if (n_streams == capacity) {
            data = realloc(data, 2 * capacity * sizeof(stream_t*));
            capacity *= 2;
        }
        data[n_streams] = stream;
        n_streams++;
    }
    free(line);
    streams->data = data;
    streams->n = n_streams;
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
    for (size_t i = 0; i < streams->n; ++i) {
        _write_stream_fp(fp, streams->data[i]);
    }
    fclose(fp);
}

const streams_t* read_streams_from_binary(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Unable to open file '%s' for reading.\n", filename);
        return NULL;
    }
    streams_t* streams = malloc(sizeof(streams_t));
    fread(&(streams->n), sizeof(size_t), 1, fp);
    streams->data = malloc(streams->n * sizeof(stream_t *));
    for (size_t i = 0; i < streams->n; i++) {
        streams->data[i] = _read_stream_fp(fp);
    }
    fclose(fp);
    return streams;
}
