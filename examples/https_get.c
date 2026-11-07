#include "pmsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PMSP_SYSTEM_CA_FILE "/etc/ssl/certs/ca-certificates.crt"

static int write_output(void *context, const unsigned char *data, size_t length)
{
    FILE *out = (FILE *)context;
    return fwrite(data, 1, length, out) == length ? 0 : -1;
}

static void usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s <host> <path> [--output file] [--port port] [--ca-file file]\n"
        "       %s <host> <path> [port] [ca-file]  (legacy form)\n"
        "\n"
        "Examples:\n"
        "  %s example.com /\n"
        "  %s example.com /index.html --output index.html\n"
        "  %s example.com / 443 /etc/ssl/certs/ca-certificates.crt\n",
        program, program, program, program, program);
}

static int parse_arguments(int argc, char **argv,
                           const char **output_path,
                           const char **port,
                           const char **ca_file)
{
    int i;
    int positional = 0;

    *output_path = NULL;
    *port = "443";
    *ca_file = PMSP_SYSTEM_CA_FILE;

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc || argv[i + 1][0] == '\0' || *output_path)
                return -1;
            *output_path = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc || argv[i + 1][0] == '\0')
                return -1;
            *port = argv[++i];
        } else if (strcmp(argv[i], "--ca-file") == 0) {
            if (i + 1 >= argc || argv[i + 1][0] == '\0')
                return -1;
            *ca_file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            return 1;
        } else if (argv[i][0] == '-') {
            return -1;
        } else if (positional == 0) {
            *port = argv[i];
            ++positional;
        } else if (positional == 1) {
            *ca_file = argv[i];
            ++positional;
        } else {
            return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *output_path;
    const char *port;
    const char *ca_file;
    FILE *out = stdout;
    char *temporary_path = NULL;
    int result;
    int parse_result;

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("PMSP %s\n", PMSP_VERSION);
        return 0;
    }

    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    parse_result = parse_arguments(argc, argv, &output_path, &port, &ca_file);
    if (parse_result != 0) {
        if (parse_result < 0)
            fprintf(stderr, "PMSP: invalid command-line arguments.\n");
        usage(argv[0]);
        return parse_result > 0 ? 0 : 2;
    }

    if (output_path != NULL) {
        size_t path_length = strlen(output_path);

        if (path_length > (size_t)-1 - 5) {
            fprintf(stderr, "PMSP: output path is too long.\n");
            return 2;
        }

        temporary_path = (char *)malloc(path_length + 5);
        if (temporary_path == NULL) {
            fprintf(stderr, "PMSP: cannot allocate output path.\n");
            return 1;
        }

        memcpy(temporary_path, output_path, path_length);
        memcpy(temporary_path + path_length, ".tmp", 5);

        out = fopen(temporary_path, "wb");
        if (out == NULL) {
            perror("PMSP: cannot open temporary output file");
            free(temporary_path);
            return 1;
        }

        fprintf(stderr, "PMSP: saving response to %s\n", output_path);
    }

    result = pmsp_https_get(argv[1], port, argv[2], ca_file,
                            write_output, out);

    if (fflush(out) != 0 && result == PMSP_OK)
        result = PMSP_ERR_OUTPUT;

    if (output_path != NULL) {
        if (fclose(out) != 0 && result == PMSP_OK)
            result = PMSP_ERR_OUTPUT;

        if (result == PMSP_OK) {
            if (rename(temporary_path, output_path) != 0) {
                perror("PMSP: cannot finalize downloaded file");
                remove(temporary_path);
                result = PMSP_ERR_OUTPUT;
            } else {
                fprintf(stderr, "PMSP: download saved to %s\n", output_path);
            }
        } else {
            remove(temporary_path);
            fprintf(stderr, "PMSP: download failed; temporary file removed.\n");
        }
        free(temporary_path);
    }

    if (result != PMSP_OK) {
        fprintf(stderr, "PMSP: request failed (code %d).\n", result);
        return 1;
    }

    return 0;
}
