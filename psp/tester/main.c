#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include "pmsp_api.h"

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
        if (pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) break;
        sceKernelDelayThread(10000);
    }
}

static void print_error(const char *label, int error)
{
    pspDebugScreenPrintf("%s: 0x%08X\n", label, (unsigned int)error);
}

int main(void)
{
    SceUID module_id;
    int status = 0;
    int start_result;
    int stop_result;
    PMSPApi api;

    pspDebugScreenInit();
    pspDebugScreenPrintf("PMSP Tester - HTTPS API test\n");
    pspDebugScreenPrintf("------------------------------\n");
    pspDebugScreenPrintf("Loading: %s\n\n", PMSP_PRX_PATH);

    module_id = sceKernelLoadModule(PMSP_PRX_PATH, 0, NULL);
    if (module_id < 0) {
        pspDebugScreenPrintf("FAILED: sceKernelLoadModule\n");
        print_error("Error", module_id);
        pspDebugScreenPrintf("\nCopy pmsp.prx to ms0:/PMSP/\nPress X or O to exit.");
        wait_for_confirm();
        sceKernelExitGame();
        return 0;
    }

    pspDebugScreenPrintf("PRX loaded. Module ID: %d\n", module_id);
    pspDebugScreenPrintf("Starting PRX and requesting API...\n");
    api.struct_size = sizeof(PMSPApi);
    api.api_version = PMSP_API_VERSION;
    api.get_version = NULL;
    api.get_status = NULL;
    api.get_last_error = NULL;
    api.https_get = NULL;

    start_result = sceKernelStartModule(module_id, sizeof(api), &api, &status, NULL);
    if (start_result < 0) {
        pspDebugScreenPrintf("FAILED: sceKernelStartModule\n");
        print_error("Error", start_result);
        sceKernelUnloadModule(module_id);
        pspDebugScreenPrintf("\nPress X or O to exit.");
        wait_for_confirm();
        sceKernelExitGame();
        return 0;
    }

    pspDebugScreenPrintf("PRX started successfully.\n");
    if (api.get_version == NULL || api.get_status == NULL ||
        api.get_last_error == NULL || api.https_get == NULL) {
        pspDebugScreenPrintf("FAILED: API table is incomplete.\n");
    } else {
        unsigned int version = api.get_version();
        pspDebugScreenPrintf("\nPMSP API detected!\n");
        pspDebugScreenPrintf("API version: %u.%u\n",
            (version >> 16) & 0xFFFF, version & 0xFFFF);
        pspDebugScreenPrintf("API table size: %u bytes\n", api.struct_size);
        pspDebugScreenPrintf("Status: %d\n", api.get_status());
        pspDebugScreenPrintf("Last error: %d\n", api.get_last_error());
        pspDebugScreenPrintf("HTTPS GET: available\n");
        if (api.get_status() == PMSP_STATUS_READY &&
            api.get_last_error() == PMSP_OK) {
            pspDebugScreenPrintf("\nSUCCESS: PMSP HTTPS API exported.\n");
        } else {
            pspDebugScreenPrintf("\nFAILED: unexpected API state.\n");
        }
    }

    pspDebugScreenPrintf("\nCaller must initialize Wi-Fi first.\n");
    pspDebugScreenPrintf("Call: api.https_get(url, output_path)\n");
    pspDebugScreenPrintf("\nPress X or O to stop and exit.");
    wait_for_confirm();

    stop_result = sceKernelStopModule(module_id, 0, NULL, &status, NULL);
    if (stop_result < 0) print_error("Warning: sceKernelStopModule", stop_result);
    else pspDebugScreenPrintf("\nPMSP PRX stopped.\n");
    sceKernelUnloadModule(module_id);
    sceKernelExitGame();
    return 0;
}
