#ifndef PMSP_H
#define PMSP_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define PMSP_VERSION "0.1.0"

typedef int (*pmsp_write_callback)(void *context, const unsigned char *data, size_t length);

enum {
    PMSP_OK = 0,
    PMSP_ERR_ARGUMENT = -1,
    PMSP_ERR_DNS = -2,
    PMSP_ERR_CONNECT = -3,
    PMSP_ERR_CA_FILE = -4,
    PMSP_ERR_TLS = -5,
    PMSP_ERR_VERIFY = -6,
    PMSP_ERR_OUTPUT = -7
};

/* One HTTPS GET. Response bytes are streamed to callback in fixed-size chunks.
 * host excludes scheme and port; path begins with '/'; NULL ca_file uses the
 * POSIX system CA bundle. This first version is POSIX-only.
 */
int pmsp_https_get(const char *host, const char *port, const char *path,
                   const char *ca_file, pmsp_write_callback callback,
                   void *context);

#ifdef __cplusplus
}
#endif
#endif
