#include <pspkernel.h>

PSP_MODULE_INFO("PMSP", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

/*
 * Initial PRX bootstrap.
 *
 * The PSPSDK PRX startup code provides module_start/module_stop and calls
 * main(). The HTTPS implementation is still POSIX-only; this module only
 * establishes the PSP PRX build/load lifecycle for now.
 */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    return 0;
}
