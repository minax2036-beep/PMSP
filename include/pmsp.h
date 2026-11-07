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
    PMSP_ERR_OUTPUT = -7,
    PMSP_ERR_HTTP = -8
};

/* One HTTPS GET. The parsed final response body (not HTTP headers or chunk
 * framing) is streamed to callback in fixed-size chunks. Follows up to five
 * HTTP 301, 302, 303, 307, and 308 redirects. HTTPS-to-HTTP redirects are
 * rejected; every redirect target gets normal TLS certificate/hostname checks.
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
