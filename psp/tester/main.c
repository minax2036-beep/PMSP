#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>

PSP_MODULE_INFO("PMSP Tester", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define PMSP_PRX_PATH "ms0:/PMSP/pmsp.prx"

static void wait_for_confirm(void)
{
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    for (;;) {
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
            break;
        }
        sceKernelDelayThread(10000);
    }
}

int main(void)
{
    SceUID module_id;
    int status = 0;
    int start_result;

    pspDebugScreenInit();
    pspDebugScreenPrintf("PMSP Tester\n");
    pspDebugScreenPrintf("------------------------------\n");
    pspDebugScreenPrintf("Loading: %s\n\n", PMSP_PRX_PATH);

    module_id = sceKernelLoadModule(PMSP_PRX_PATH, 0, NULL);
    if (module_id < 0) {
        pspDebugScreenPrintf("FAILED: sceKernelLoadModule\n");
        pspDebugScreenPrintf("Error: 0x%08X\n", (unsigned int)module_id);
        pspDebugScreenPrintf("\nCopy pmsp.prx to ms0:/PMSP/\n");
        pspDebugScreenPrintf("Press X or O to exit.");
        wait_for_confirm();
        sceKernelExitGame();
        return 0;
    }

    pspDebugScreenPrintf("PRX loaded. Module ID: %d\n", module_id);
    pspDebugScreenPrintf("Starting module...\n");

    start_result = sceKernelStartModule(module_id, 0, NULL, &status, NULL);
    if (start_result < 0) {
        pspDebugScreenPrintf("FAILED: sceKernelStartModule\n");
        pspDebugScreenPrintf("Error: 0x%08X\n", (unsigned int)start_result);
        sceKernelUnloadModule(module_id);
    } else {
        pspDebugScreenPrintf("SUCCESS: PMSP PRX started.\n");
        pspDebugScreenPrintf("Module status: %d\n", status);
        pspDebugScreenPrintf("\nThis is the PRX bootstrap test.\n");
        pspDebugScreenPrintf("HTTPS support is not wired into\n");
        pspDebugScreenPrintf("the PSP PRX yet.\n");
        pspDebugScreenPrintf("\nPress X or O to stop and exit.");

        wait_for_confirm();
        sceKernelStopModule(module_id, 0, NULL, &status, NULL);
        sceKernelUnloadModule(module_id);
    }

    sceKernelExitGame();
    return 0;
}
