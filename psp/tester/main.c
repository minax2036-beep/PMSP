#include <pspkernel.h>
#include <pspsdk.h>
#include <pspsysmem.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility_netmodules.h>
#include "pmsp_api.h"

PSP_MODULE_INFO("PMSP Net Init Test", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define PMSP_PRX_PATH "ms0:/PSP/GAME/PMSP/pmsp.prx"

static const char *g_stage = "Startup";
static SceSize g_user_free_before;
static SceSize g_user_free_after_prx;
static SceSize g_user_free_after_unload;
static SceSize g_user_free_after_modules;
static int g_inet_initialized;

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;

    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int callback_id;

    (void)args;
    (void)argp;

    callback_id = sceKernelCreateCallback(
        "PMSP Exit Callback", exit_callback, NULL);
    if (callback_id >= 0) {
        sceKernelRegisterExitCallback(callback_id);
    }

    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    SceUID thread_id = sceKernelCreateThread(
        "PMSP Callback Thread",
        callback_thread,
        0x11,
        0x1000,
        PSP_THREAD_ATTR_USER,
        NULL);

    if (thread_id >= 0) {
        sceKernelStartThread(thread_id, 0, NULL);
    }
}

static unsigned int read_buttons(void)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);
    return pad.Buttons;
}

static void wait_for_cross(void)
{
    while (!(read_buttons() & PSP_CTRL_CROSS)) {
        sceKernelDelayThreadCB(10000);
    }
    while (read_buttons() & PSP_CTRL_CROSS) {
        sceKernelDelayThreadCB(10000);
    }
}

static void show_result(int result)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("PMSP - MEMORY / NET TEST\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("Stage: %s\n", g_stage);
    pspDebugScreenPrintf("Result: 0x%08X\n\n", (unsigned int)result);

    pspDebugScreenPrintf("User free memory (bytes):\n");
    pspDebugScreenPrintf("Before PRX: %u\n", (unsigned int)g_user_free_before);
    pspDebugScreenPrintf("After PRX:  %u\n", (unsigned int)g_user_free_after_prx);
    pspDebugScreenPrintf("After unload: %u\n", (unsigned int)g_user_free_after_unload);
    pspDebugScreenPrintf("After modules: %u\n\n", (unsigned int)g_user_free_after_modules);

    pspDebugScreenPrintf("Kernel free: %u bytes\n",
        (unsigned int)sceKernelTotalFreeMemSize());
    pspDebugScreenPrintf("Kernel largest block: %u bytes\n\n",
        (unsigned int)sceKernelMaxFreeMemSize());

    if (result < 0) {
        pspDebugScreenPrintf("TEST FAILED\n");
    } else {
        pspDebugScreenPrintf("NETWORK INIT SUCCESS\n");
        pspDebugScreenPrintf("pspSdkInetInit() returned 0.\n");
    }

    pspDebugScreenPrintf("\nX: Exit test\n");
    pspDebugScreenPrintf("HOME: PSP system menu\n");
}

static void unload_net_modules(int inet_loaded, int common_loaded)
{
    if (inet_loaded) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    }
    if (common_loaded) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    }
}

int main(void)
{
    SceUID module_id;
    int result;
    int status = 0;
    int stop_result;
    int common_loaded = 0;
    int inet_loaded = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenInit();
    setup_callbacks();

    g_user_free_before = pspSdkTotalFreeUserMemSize();

    pspDebugScreenPrintf("PMSP - MEMORY / NET TEST\n");
    pspDebugScreenPrintf("------------------------------\n");
    pspDebugScreenPrintf("User free before PRX: %u bytes\n",
        (unsigned int)g_user_free_before);
    pspDebugScreenPrintf("Loading PMSP before network modules...\n");

    /*
     * Phase 1: test the PRX with no network modules loaded.
     * This reverses the previous experiment's order.
     */
    g_stage = "sceKernelLoadModule(PMSP)";
    module_id = sceKernelLoadModule(PMSP_PRX_PATH, 0, NULL);
    if (module_id < 0) {
        result = module_id;
        goto finished;
    }

    g_stage = "sceKernelStartModule(PMSP)";
    result = sceKernelStartModule(module_id, 0, NULL, &status, NULL);
    g_user_free_after_prx = pspSdkTotalFreeUserMemSize();
    if (result < 0) {
        sceKernelUnloadModule(module_id);
        g_user_free_after_unload = pspSdkTotalFreeUserMemSize();
        goto finished;
    }

    g_stage = "sceKernelStopModule(PMSP)";
    stop_result = sceKernelStopModule(module_id, 0, NULL, &status, NULL);
    if (stop_result < 0) {
        result = stop_result;
        goto finished;
    }

    g_stage = "sceKernelUnloadModule(PMSP)";
    result = sceKernelUnloadModule(module_id);
    if (result < 0) {
        goto finished;
    }

    g_user_free_after_unload = pspSdkTotalFreeUserMemSize();

    /*
     * Phase 2: only after PMSP has been stopped and unloaded,
     * load the official network modules.
     */
    g_stage = "sceUtilityLoadNetModule(COMMON)";
    result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    if (result < 0) {
        goto finished;
    }
    common_loaded = 1;

    g_stage = "sceUtilityLoadNetModule(INET)";
    result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    if (result < 0) {
        goto finished;
    }
    inet_loaded = 1;

    g_user_free_after_modules = pspSdkTotalFreeUserMemSize();

    /*
     * Phase 3: run the PSPSDK Internet initialization helper.
     * Do not open Netconf or attempt HTTPS in this diagnostic build.
     */
    g_stage = "pspSdkInetInit";
    result = pspSdkInetInit();
    if (result == 0) {
        g_inet_initialized = 1;
    }

finished:
    if (result < 0) {
        /* Preserve the failing stage and error code on screen. */
        unload_net_modules(inet_loaded, common_loaded);
    }

    show_result(result);

    for (;;) {
        wait_for_cross();

        if (g_inet_initialized) {
            pspSdkInetTerm();
            g_inet_initialized = 0;
        }
        unload_net_modules(inet_loaded, common_loaded);
        sceKernelExitGame();
    }

    return 0;
}
