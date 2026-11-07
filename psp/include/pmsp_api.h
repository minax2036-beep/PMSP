#ifndef PMSP_API_H
#define PMSP_API_H

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMSP_API_VERSION_MAJOR 1
#define PMSP_API_VERSION_MINOR 0
#define PMSP_API_VERSION ((PMSP_API_VERSION_MAJOR << 16) | PMSP_API_VERSION_MINOR)

#define PMSP_STATUS_STOPPED 0
#define PMSP_STATUS_READY   1

#define PMSP_OK                 0
#define PMSP_ERROR_INVALID_ARG -1

typedef unsigned int (*PMSPGetVersionFn)(void);
typedef int (*PMSPGetStatusFn)(void);
typedef int (*PMSPGetLastErrorFn)(void);

typedef struct PMSPApi {
    unsigned int struct_size;
    unsigned int api_version;
    PMSPGetVersionFn get_version;
    PMSPGetStatusFn get_status;
    PMSPGetLastErrorFn get_last_error;
} PMSPApi;

/*
 * The application passes a PMSPApi structure to sceKernelStartModule().
 * PMSP fills it during module_start(), allowing the caller to invoke the
 * functions after the PRX has been started.
 *
 * The same functions are also listed in exports.exp for normal PRX imports.
 */

#ifdef __cplusplus
}
#endif

#endif
