#ifndef PMSP_API_H
#define PMSP_API_H

#ifdef __cplusplus
extern "C" {
#endif

#define PMSP_API_VERSION_MAJOR 1
#define PMSP_API_VERSION_MINOR 1
#define PMSP_API_VERSION ((PMSP_API_VERSION_MAJOR << 16) | PMSP_API_VERSION_MINOR)

#define PMSP_STATUS_STOPPED 0
#define PMSP_STATUS_READY 1

#define PMSP_OK 0
#define PMSP_ERROR_INVALID_ARG -1
#define PMSP_ERROR_NETWORK -2
#define PMSP_ERROR_TLS -3
#define PMSP_ERROR_HTTP -4
#define PMSP_ERROR_IO -5

/*
 * These functions are exported by the PMSP PRX.
 * The Tester links the matching weak-import stubs generated from exports.exp.
 */
unsigned int pmspGetVersion(void);
int pmspGetStatus(void);
int pmspGetLastError(void);
int pmspHttpsGet(const char *url, const char *output_path);

/*
 * pmspHttpsGet accepts only https:// URLs and saves the response body to a file.
 * The caller must initialize the PSP network stack and connect to Wi-Fi first.
 * Automatic redirects are disabled in this initial backend.
 */

#ifdef __cplusplus
}
#endif

#endif
