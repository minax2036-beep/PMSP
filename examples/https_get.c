#include "pmsp.h"
#include <stdio.h>
#include <string.h>

#define PMSP_SYSTEM_CA_FILE "/etc/ssl/certs/ca-certificates.crt"

static int write_stdout(void *context, const unsigned char *data, size_t length)
{
    FILE *out = (FILE *)context;
    return fwrite(data, 1, length, out) == length ? 0 : -1;
}

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s <host> <path> [port] [ca-file]\n", program);
    fprintf(stderr, "Example: %s example.com /\n", program);
}

int main(int argc, char **argv)
{
    const char *port = "443";
    const char *ca_file = PMSP_SYSTEM_CA_FILE;
    int result;

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("PMSP %s\n", PMSP_VERSION);
        return 0;
    }
    if (argc < 3 || argc > 5) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 4) port = argv[3];
    if (argc >= 5) ca_file = argv[4];

    result = pmsp_https_get(argv[1], port, argv[2], ca_file, write_stdout, stdout);
    if (result != PMSP_OK) {
        fprintf(stderr, "PMSP: request failed (code %d).\n", result);
        return 1;
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
