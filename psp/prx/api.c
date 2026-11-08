#include "pmsp_internal.h"

int g_status = PMSP_STATUS_STOPPED;
int g_last_error = PMSP_OK;

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
