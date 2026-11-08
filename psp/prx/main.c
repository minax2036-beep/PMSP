#include <pspkernel.h>
#include "pmsp_internal.h"

PSP_MODULE_INFO("PMSP", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    g_last_error = PMSP_OK;
    g_status = PMSP_STATUS_READY;
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    g_status = PMSP_STATUS_STOPPED;
    return 0;
}
