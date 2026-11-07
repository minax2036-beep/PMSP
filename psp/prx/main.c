#include <pspkernel.h>

PSP_MODULE_INFO("PMSP", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

/*
 * Initial PRX bootstrap.
 *
 * The HTTPS implementation is still POSIX-only. This module establishes
 * the PSP PRX build/load lifecycle; the PSP networking and Mbed TLS port
 * will be integrated in a later step.
 */
int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}
