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

static unsigned int read_buttons(void)
{
    SceCtrlData pad;

    sceCtrlReadBufferPositive(&pad, 1);
    return pad.Buttons;
}

static unsigned int wait_for_button(void)
{
    unsigned int buttons;

    for (;;) {
        buttons = read_buttons();
        if (buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
            break;
        }
        sceKernelDelayThread(10000);
    }

    while (read_buttons() & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
        sceKernelDelayThread(10000);
    }

    return buttons;
}

static void print_error(const char *label, int error)
{
    pspDebugScreenPrintf("%s: 0x%08X\n", label, (unsigned int)error);
}

static void show_error(const char *title, const char *message, int error)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("PMSP - ERROR\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("%s\n\n", title);
    pspDebugScreenPrintf("%s\n\n", message);
    print_error("Error code", error);
    pspDebugScreenPrintf("\nX: Retry / continue\n");
    pspDebugScreenPrintf("O: Back to menu\n");
}

static int init_network_stack(void)
{
    int result;

    if (!g_net_modules_loaded) {
        result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
        if (result < 0) return result;

        result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
        if (result < 0) {
            sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
            return result;
        }
        g_net_modules_loaded = 1;
    }

    if (!g_net_stack_started) {
        result = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
        if (result < 0) return result;
        g_net_stack_started = 1;
    }

    if (!g_net_inet_started) {
        result = sceNetInetInit();
        if (result < 0) return result;
        g_net_inet_started = 1;
    }

    if (!g_net_apctl_started) {
        result = sceNetApctlInit(0x8000, 48);
        if (result < 0) return result;
        g_net_apctl_started = 1;
    }

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
    int state = 0;

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

    result = sceUtilityNetconfInitStart(&data);
    if (result < 0) return result;

    for (;;) {
        status = sceUtilityNetconfGetStatus();

        if (status == PSP_UTILITY_DIALOG_VISIBLE) {
            sceUtilityNetconfUpdate(1);
        } else if (status == PSP_UTILITY_DIALOG_QUIT) {
            sceUtilityNetconfShutdownStart();
        } else if (status == PSP_UTILITY_DIALOG_FINISHED) {
            break;
        }

        sceDisplayWaitVblankStart();
        sceKernelDelayThread(1000);
    }

    while (sceUtilityNetconfGetStatus() != PSP_UTILITY_DIALOG_NONE) {
        sceDisplayWaitVblankStart();
        sceKernelDelayThread(1000);
    }

    result = sceNetApctlGetState(&state);
    if (result < 0) return result;
    if (state != 4) return -1;

    return 0;
}

static void show_main_menu(unsigned int version)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("PMSP Tester - HTTPS API\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("PMSP PRX: loaded\n");
    pspDebugScreenPrintf("API version: %u.%u\n",
        (version >> 16) & 0xFFFF, version & 0xFFFF);
    pspDebugScreenPrintf("API status: %d\n", pmspGetStatus());
    pspDebugScreenPrintf("Last error: %d\n\n", pmspGetLastError());
    pspDebugScreenPrintf("X: Connect to Wi-Fi\n");
    pspDebugScreenPrintf("O: Return to PSP home (XMB)\n");
}

static void show_wifi_success(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("PMSP - NETWORK READY\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("Wi-Fi connection is ready.\n");
    pspDebugScreenPrintf("PMSP API is available.\n\n");
    pspDebugScreenPrintf("The HTTPS GET test can now be\n");
    pspDebugScreenPrintf("started by the application.\n\n");
    pspDebugScreenPrintf("X: Open Wi-Fi settings again\n");
    pspDebugScreenPrintf("O: Back to PMSP menu\n");
}

int main(void)
{
    SceUID module_id;
    int status = 0;
    int result;
    int stop_result;
    unsigned int version;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenInit();

    pspDebugScreenPrintf("PMSP Tester\n");
    pspDebugScreenPrintf("------------------------------\n");
    pspDebugScreenPrintf("Loading: %s\n\n", PMSP_PRX_PATH);

    module_id = sceKernelLoadModule(PMSP_PRX_PATH, 0, NULL);
    if (module_id < 0) {
        show_error("PRX load failed",
            "Check that pmsp.prx is beside EBOOT.PBP.", module_id);
        wait_for_button();
        sceKernelExitGame();
        return 0;
    }

    result = sceKernelStartModule(module_id, 0, NULL, &status, NULL);
    if (result < 0) {
        show_error("PRX start failed", "The PMSP module could not start.", result);
        sceKernelUnloadModule(module_id);
        wait_for_button();
        sceKernelExitGame();
        return 0;
    }

    version = pmspGetVersion();

    for (;;) {
        unsigned int buttons;

        show_main_menu(version);
        buttons = wait_for_button();

        if (buttons & PSP_CTRL_CIRCLE) {
            break;
        }

        if (buttons & PSP_CTRL_CROSS) {
            result = run_network_dialog();
            if (result < 0) {
                show_error("Wi-Fi connection failed",
                    "Check the WLAN switch and access point settings.", result);
                buttons = wait_for_button();
                if (buttons & PSP_CTRL_CIRCLE) {
                    continue;
                }
            } else {
                show_wifi_success();
                buttons = wait_for_button();
                if (buttons & PSP_CTRL_CIRCLE) {
                    continue;
                }
            }
        }
    }

    term_network();

    stop_result = sceKernelStopModule(module_id, 0, NULL, &status, NULL);
    if (stop_result < 0) {
        pspDebugScreenClear();
        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("Warning: could not stop PMSP PRX.\n");
        print_error("Error", stop_result);
        wait_for_button();
    }

    sceKernelUnloadModule(module_id);
    sceKernelExitGame();
    return 0;
}
