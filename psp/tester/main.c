#include <pspkernel.h>
#include <string.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include "pmsp_api.h"

PSP_MODULE_INFO("PMSP Tester", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define PMSP_PRX_PATH "ms0:/PSP/GAME/PMSP/pmsp.prx"

static int g_net_modules_loaded;
static int g_net_stack_started;
static int g_net_inet_started;
static int g_net_apctl_started;

static const char *g_failure_stage = "Not started";
static int g_dialog_last_status = -1;
static int g_apctl_state = -1;

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
    SceUID thread_id;

    thread_id = sceKernelCreateThread(
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

static void print_error(const char *label, int error)
{
    pspDebugScreenPrintf("%s: 0x%08X\n", label, (unsigned int)error);
}

static void show_error(const char *title, const char *message, int error)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("PMSP - DIAGNOSTIC ERROR\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("%s\n", title);
    pspDebugScreenPrintf("%s\n\n", message);
    pspDebugScreenPrintf("Failed stage:\n%s\n", g_failure_stage);
    print_error("Return code", error);
    pspDebugScreenPrintf("Netconf status: 0x%08X\n",
        (unsigned int)g_dialog_last_status);
    pspDebugScreenPrintf("APCTL state: %d\n\n", g_apctl_state);
    pspDebugScreenPrintf("X: Retry Wi-Fi connection\n");
    pspDebugScreenPrintf("HOME: PSP system menu\n");
}

static int init_network_stack(void)
{
    int result;

    if (!g_net_modules_loaded) {
        g_failure_stage = "sceUtilityLoadNetModule(COMMON)";
        result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
        if (result < 0) return result;

        g_failure_stage = "sceUtilityLoadNetModule(INET)";
        result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
        if (result < 0) {
            sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
            return result;
        }
        g_net_modules_loaded = 1;
    }

    if (!g_net_stack_started) {
        g_failure_stage = "sceNetInit";
        result = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
        if (result < 0) return result;
        g_net_stack_started = 1;
    }

    if (!g_net_inet_started) {
        g_failure_stage = "sceNetInetInit";
        result = sceNetInetInit();
        if (result < 0) return result;
        g_net_inet_started = 1;
    }

    if (!g_net_apctl_started) {
        g_failure_stage = "sceNetApctlInit";
        result = sceNetApctlInit(0x8000, 48);
        if (result < 0) return result;
        g_net_apctl_started = 1;
    }

    g_failure_stage = "Network stack initialized";
    return 0;
}

static void term_network(void)
{
    if (g_net_apctl_started) {
        sceNetApctlTerm();
        g_net_apctl_started = 0;
    }
    if (g_net_inet_started) {
        sceNetInetTerm();
        g_net_inet_started = 0;
    }
    if (g_net_stack_started) {
        sceNetTerm();
        g_net_stack_started = 0;
    }
    if (g_net_modules_loaded) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        g_net_modules_loaded = 0;
    }
}

static int run_network_dialog(void)
{
    pspUtilityNetconfData data;
    int result;
    int status;

    g_failure_stage = "Network stack initialization";
    g_dialog_last_status = -1;
    g_apctl_state = -1;

    result = init_network_stack();
    if (result < 0) return result;

    memset(&data, 0, sizeof(data));
    data.base.size = sizeof(data);
    data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    data.base.graphicsThread = 17;
    data.base.accessThread = 19;
    data.base.fontThread = 18;
    data.base.soundThread = 16;
    data.action = PSP_NETCONF_ACTION_CONNECTAP;
    data.adhocparam = NULL;
    data.hotspot = 0;
    data.hotspot_connected = 0;
    data.wifisp = 0;

    g_failure_stage = "sceUtilityNetconfInitStart";
    result = sceUtilityNetconfInitStart(&data);
    if (result < 0) return result;

    for (;;) {
        g_failure_stage = "sceUtilityNetconfGetStatus";
        status = sceUtilityNetconfGetStatus();
        g_dialog_last_status = status;

        if (status == PSP_UTILITY_DIALOG_VISIBLE) {
            g_failure_stage = "sceUtilityNetconfUpdate";
            sceUtilityNetconfUpdate(1);
        } else if (status == PSP_UTILITY_DIALOG_QUIT) {
            g_failure_stage = "sceUtilityNetconfShutdownStart";
            result = sceUtilityNetconfShutdownStart();
            if (result < 0) return result;
        } else if (status == PSP_UTILITY_DIALOG_FINISHED) {
            break;
        } else if (status == PSP_UTILITY_DIALOG_NONE) {
            g_failure_stage = "Netconf dialog disappeared unexpectedly";
            return -1;
        }

        sceDisplayWaitVblankStart();
        sceKernelDelayThreadCB(1000);
    }

    /* FINISHED follows the QUIT -> ShutdownStart transition.
       Wait for the utility dialog to fully return to NONE. */
    for (;;) {
        g_failure_stage = "sceUtilityNetconfGetStatus(wait NONE)";
        status = sceUtilityNetconfGetStatus();
        g_dialog_last_status = status;
        if (status == PSP_UTILITY_DIALOG_NONE) break;

        sceDisplayWaitVblankStart();
        sceKernelDelayThreadCB(1000);
    }

    g_failure_stage = "sceNetApctlGetState";
    result = sceNetApctlGetState(&g_apctl_state);
    if (result < 0) return result;

    if (g_apctl_state != 4) {
        g_failure_stage = "sceNetApctlGetState: not connected (state != 4)";
        return -1;
    }

    g_failure_stage = "Wi-Fi connected";
    return 0;
}

static void show_wifi_success(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("PMSP - NETWORK READY\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("Wi-Fi connection is ready.\n");
    pspDebugScreenPrintf("APCTL state: %d\n", g_apctl_state);
    pspDebugScreenPrintf("PMSP API status: %d\n", pmspGetStatus());
    pspDebugScreenPrintf("Last error: %d\n\n", pmspGetLastError());
    pspDebugScreenPrintf("X: Open Wi-Fi settings again\n");
    pspDebugScreenPrintf("HOME: PSP system menu\n");
}

int main(void)
{
    SceUID module_id;
    int status = 0;
    int result;
    int stop_result;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenInit();
    setup_callbacks();

    pspDebugScreenPrintf("PMSP Tester\n");
    pspDebugScreenPrintf("------------------------------\n");
    pspDebugScreenPrintf("Loading: %s\n\n", PMSP_PRX_PATH);

    module_id = sceKernelLoadModule(PMSP_PRX_PATH, 0, NULL);
    if (module_id < 0) {
        g_failure_stage = "sceKernelLoadModule(PMSP PRX)";
        show_error("PRX load failed",
            "Check the PRX path and file placement.", module_id);
        for (;;) wait_for_cross();
    }

    g_failure_stage = "sceKernelStartModule(PMSP PRX)";
    result = sceKernelStartModule(module_id, 0, NULL, &status, NULL);
    if (result < 0) {
        show_error("PRX start failed",
            "The PMSP module could not start.", result);
        sceKernelUnloadModule(module_id);
        for (;;) wait_for_cross();
    }

    /* Automatically show the official PSP network dialog at startup. */
    for (;;) {
        result = run_network_dialog();

        if (result < 0) {
            show_error("Wi-Fi setup failed",
                "The failed stage is shown above.", result);
        } else {
            show_wifi_success();
        }

        /* CROSS retries. HOME remains handled by the PSP system UI. */
        wait_for_cross();
    }

    term_network();
    stop_result = sceKernelStopModule(module_id, 0, NULL, &status, NULL);
    if (stop_result >= 0) {
        sceKernelUnloadModule(module_id);
    }

    sceKernelExitGame();
    return 0;
}
