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
        "       %s github-tag <owner/repo> <tag> [--output file] [--ca-file file]\n"
        "       %s github-release <owner/repo> [--output file] [--ca-file file]\n"
        "       %s <host> <path> [port] [ca-file]  (legacy form)\n"
        "\n"
        "Examples:\n"
        "  %s example.com /\n"
        "  %s example.com /index.html --output index.html\n"
        "  %s github-tag minax2036-beep/PMSP v0.1.0 --output PMSP-v0.1.0.zip\n"
        "  %s github-release minax2036-beep/PMSP --output latest-release.json\n",
        program, program, program, program, program, program, program);
}

static int parse_arguments(int argc, char **argv, int first_option,
                           const char **output_path,
                           const char **port,
                           const char **ca_file)
{
    int i;
    int positional = 0;

    *output_path = NULL;
    *port = "443";
    *ca_file = PMSP_SYSTEM_CA_FILE;

    for (i = first_option; i < argc; ++i) {
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


static int is_repo_component(const char *text, size_t length)
{
    size_t i;
    if (length == 0 || (length == 1 && text[0] == '.') ||
        (length == 2 && text[0] == '.' && text[1] == '.')) return 0;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return 0;
    }
    return 1;
}

static char *make_github_tag_path(const char *repository, const char *tag)
{
    const char *slash;
    size_t owner_length, repo_length, tag_length, capacity, used, i;
    char *path;
    static const char hex[] = "0123456789ABCDEF";
    if (!repository || !tag || !tag[0]) return NULL;
    slash = strchr(repository, '/');
    if (!slash || strchr(slash + 1, '/') != NULL) return NULL;
    owner_length = (size_t)(slash - repository);
    repo_length = strlen(slash + 1);
    tag_length = strlen(tag);
    if (owner_length > 100 || repo_length > 100 || tag_length > 256 ||
        !is_repo_component(repository, owner_length) ||
        !is_repo_component(slash + 1, repo_length)) return NULL;
    if (tag_length > ((size_t)-1 - owner_length - repo_length - 64) / 3) return NULL;
    capacity = owner_length + repo_length + tag_length * 3 + 64;
    path = (char *)malloc(capacity);
    if (!path) return NULL;
    used = (size_t)snprintf(path, capacity, "/%.*s/%s/archive/refs/tags/",
                            (int)owner_length, repository, slash + 1);
    if (used >= capacity) { free(path); return NULL; }
    for (i = 0; i < tag_length; ++i) {
        unsigned char c = (unsigned char)tag[i];
        int safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
        if (safe) path[used++] = (char)c;
        else {
            path[used++] = '%';
            path[used++] = hex[c >> 4];
            path[used++] = hex[c & 15];
        }
    }
    memcpy(path + used, ".zip", sizeof(".zip"));
    return path;
}

static char *make_github_release_path(const char *repository)
{
    const char *slash;
    size_t owner_length, repo_length, capacity;
    char *path;

    if (!repository) return NULL;
    slash = strchr(repository, '/');
    if (!slash || strchr(slash + 1, '/') != NULL) return NULL;

    owner_length = (size_t)(slash - repository);
    repo_length = strlen(slash + 1);
    if (owner_length > 100 || repo_length > 100 ||
        !is_repo_component(repository, owner_length) ||
        !is_repo_component(slash + 1, repo_length)) return NULL;

    capacity = owner_length + repo_length + 40;
    path = (char *)malloc(capacity);
    if (!path) return NULL;

    if (snprintf(path, capacity, "/repos/%.*s/%s/releases/latest",
                 (int)owner_length, repository, slash + 1) < 0) {
        free(path);
        return NULL;
    }
    return path;
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
    int github_tag_mode = 0;
    int github_release_mode = 0;
    const char *request_host;
    const char *request_path;
    char *github_tag_path = NULL;
    char *github_release_path = NULL;

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("PMSP %s\n", PMSP_VERSION);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "github-tag") == 0) {
        if (argc < 4) { usage(argv[0]); return 2; }
        github_tag_path = make_github_tag_path(argv[2], argv[3]);
        if (!github_tag_path) {
            fprintf(stderr, "PMSP: invalid repository or tag name.\n");
            return 2;
        }
        github_tag_mode = 1;
        request_host = "github.com";
        request_path = github_tag_path;
    } else if (argc >= 2 && strcmp(argv[1], "github-release") == 0) {
        if (argc < 3) { usage(argv[0]); return 2; }
        github_release_path = make_github_release_path(argv[2]);
        if (!github_release_path) {
            fprintf(stderr, "PMSP: invalid repository name.\n");
            return 2;
        }
        github_release_mode = 1;
        request_host = "api.github.com";
        request_path = github_release_path;
    } else {
        if (argc < 3) { usage(argv[0]); return 2; }
        request_host = argv[1];
        request_path = argv[2];
    }

    parse_result = parse_arguments(argc, argv, github_tag_mode ? 4 : 3,
                                   &output_path, &port, &ca_file);
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

    result = pmsp_https_get(request_host, port, request_path, ca_file,
                            write_output, out);
    free(github_tag_path);
    free(github_release_path);

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
