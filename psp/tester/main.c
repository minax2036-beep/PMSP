#include <pspkernel.h>
#include <pspsdk.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include "pmsp_api.h"

PSP_MODULE_INFO("PMSP Net Init Test", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define PMSP_PRX_PATH "ms0:/PSP/GAME/PMSP/pmsp.prx"

static const char *g_stage = "Startup";

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

static void show_result(const char *title, int result)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("PMSP - MINIMAL NET TEST\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("Stage: %s\n", g_stage);
    pspDebugScreenPrintf("Result: 0x%08X\n\n", (unsigned int)result);

    if (result < 0) {
        pspDebugScreenPrintf("NETWORK INIT FAILED\n");
        pspDebugScreenPrintf("The failing stage is shown above.\n\n");
    } else {
        pspDebugScreenPrintf("NETWORK INIT SUCCESS\n");
        pspDebugScreenPrintf("PSPSDK pspSdkInetInit() returned 0.\n\n");
    }

    pspDebugScreenPrintf("X: Exit test\n");
    pspDebugScreenPrintf("HOME: PSP system menu\n");
}

int main(void)
{
    SceUID module_id;
    int result;
    int status = 0;
    int stop_result;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenInit();
    setup_callbacks();

    pspDebugScreenPrintf("PMSP - MINIMAL NET TEST\n");
    pspDebugScreenPrintf("------------------------------\n");

    g_stage = "Load NET_MODULE_COMMON";
    pspDebugScreenPrintf("Loading COMMON...\n");
    result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    if (result < 0) {
        show_result(g_stage, result);
        for (;;) wait_for_cross();
    }

    g_stage = "Load NET_MODULE_INET";
    pspDebugScreenPrintf("Loading INET...\n");
    result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    if (result < 0) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        show_result(g_stage, result);
        for (;;) wait_for_cross();
    }

    /*
     * Deliberately load and immediately unload PMSP.
     * The network initialization test then runs with no PMSP PRX active.
     */
    g_stage = "Load PMSP PRX";
    pspDebugScreenPrintf("Loading PMSP PRX...\n");
    module_id = sceKernelLoadModule(PMSP_PRX_PATH, 0, NULL);
    if (module_id < 0) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        show_result(g_stage, module_id);
        for (;;) wait_for_cross();
    }

    g_stage = "Start PMSP PRX";
    result = sceKernelStartModule(module_id, 0, NULL, &status, NULL);
    if (result < 0) {
        sceKernelUnloadModule(module_id);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        show_result(g_stage, result);
        for (;;) wait_for_cross();
    }

    g_stage = "Stop PMSP PRX";
    stop_result = sceKernelStopModule(module_id, 0, NULL, &status, NULL);
    if (stop_result < 0) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        show_result(g_stage, stop_result);
        for (;;) wait_for_cross();
    }

    g_stage = "Unload PMSP PRX";
    result = sceKernelUnloadModule(module_id);
    if (result < 0) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        show_result(g_stage, result);
        for (;;) wait_for_cross();
    }

    /*
     * This is the PSPSDK helper used by its own networking samples.
     * The PRX has already been stopped and unloaded at this point.
     */
    g_stage = "pspSdkInetInit";
    result = pspSdkInetInit();

    show_result(g_stage, result);

    /* This diagnostic build intentionally does not open Netconf or HTTPS.
       It isolates the SDK network initialization call only. */
    for (;;) {
        wait_for_cross();
        pspSdkInetTerm();
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        sceKernelExitGame();
    }

    return 0;
}
