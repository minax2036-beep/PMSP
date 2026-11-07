#include <pspkernel.h>
#include "pmsp_api.h"

PSP_MODULE_INFO("PMSP", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

static int g_status = PMSP_STATUS_STOPPED;
static int g_last_error = PMSP_OK;

unsigned int pmspGetVersion(void)
{
    return PMSP_API_VERSION;
}

int pmspGetStatus(void)
{
    return g_status;
}

int pmspGetLastError(void)
{
    return g_last_error;
}

/*
 * The caller passes a PMSPApi structure as module_start() arguments.
 * Function pointers are filled only after validating the structure size.
 */
int module_start(SceSize args, void *argp)
{
    PMSPApi *api;

    if (argp == NULL || args < sizeof(PMSPApi)) {
        g_last_error = PMSP_ERROR_INVALID_ARG;
        return PMSP_ERROR_INVALID_ARG;
    }

    api = (PMSPApi *)argp;

    api->struct_size = sizeof(PMSPApi);
    api->api_version = PMSP_API_VERSION;
    api->get_version = pmspGetVersion;
    api->get_status = pmspGetStatus;
    api->get_last_error = pmspGetLastError;

    g_last_error = PMSP_OK;
    g_status = PMSP_STATUS_READY;

    return PMSP_OK;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    g_status = PMSP_STATUS_STOPPED;
    return 0;
}
