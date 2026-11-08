#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <psputility.h>
#include <psputility_netconf.h>
#include <psputility_netmodules.h>
#include <string.h>

PSP_MODULE_INFO("PMSP Tester", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

static pspUtilityNetconfData g_netconf;
static struct pspUtilityNetconfAdhoc g_adhoc;

static int g_common_loaded;
static int g_inet_loaded;
static int g_last_error;

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

static int load_network_modules(void)
{
    int result;

    if (!g_common_loaded) {
        result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
        if (result < 0) {
            return result;
        }
        g_common_loaded = 1;
    }

    if (!g_inet_loaded) {
        result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
        if (result < 0) {
            return result;
        }
        g_inet_loaded = 1;
    }

    return 0;
}

static int open_wifi_dialog(void)
{
    int result;
    int shutdown_started = 0;

    result = load_network_modules();
    if (result < 0) {
        return result;
    }

    memset(&g_netconf, 0, sizeof(g_netconf));
    memset(&g_adhoc, 0, sizeof(g_adhoc));

    g_netconf.base.size = sizeof(g_netconf);
    g_netconf.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    g_netconf.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    g_netconf.base.graphicsThread = 17;
    g_netconf.base.accessThread = 19;
    g_netconf.base.fontThread = 18;
    g_netconf.base.soundThread = 16;
    g_netconf.action = PSP_NETCONF_ACTION_CONNECTAP;
    g_netconf.adhocparam = &g_adhoc;

    result = sceUtilityNetconfInitStart(&g_netconf);
    if (result < 0) {
        return result;
    }

    for (;;) {
        int status = sceUtilityNetconfGetStatus();

        if (status == PSP_UTILITY_DIALOG_NONE) {
            break;
        }

        if (status == PSP_UTILITY_DIALOG_VISIBLE) {
            result = sceUtilityNetconfUpdate(1);
            if (result < 0) {
                return result;
            }
        } else if (status == PSP_UTILITY_DIALOG_QUIT &&
                   !shutdown_started) {
            result = sceUtilityNetconfShutdownStart();
            if (result < 0) {
                return result;
            }
            shutdown_started = 1;
        }

        /*
         * Do not return on FINISHED. The utility remains busy until
         * GetStatus() reports NONE, and cannot be initialized again
         * before that transition completes.
         */
        sceDisplayWaitVblankStart();
        sceKernelDelayThread(1000);
    }

    return 0;
}

static void show_tester_screen(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);

    pspDebugScreenPrintf("PMSP TESTER\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("Wi-Fi setup dialog has returned.\n\n");
    pspDebugScreenPrintf("CROSS: Open Wi-Fi settings again\n");
    pspDebugScreenPrintf("HOME: PSP system exit menu\n");
}

int main(void)
{
    SceCtrlData pad;
    int previous_buttons = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    pspDebugScreenInit();
    setup_callbacks();

    pspDebugScreenPrintf("PMSP TESTER\n");
    pspDebugScreenPrintf("------------------------------\n");
    pspDebugScreenPrintf("Opening PSP Wi-Fi settings...\n");

    g_last_error = open_wifi_dialog();

    show_tester_screen();
    if (g_last_error < 0) {
        pspDebugScreenPrintf("\nWi-Fi dialog error: 0x%08X\n",
            (unsigned int)g_last_error);
    }

    for (;;) {
        unsigned int buttons;

        sceCtrlReadBufferPositive(&pad, 1);
        buttons = pad.Buttons;

        if ((buttons & PSP_CTRL_CROSS) &&
            !(previous_buttons & PSP_CTRL_CROSS)) {
            pspDebugScreenPrintf("\nOpening Wi-Fi settings...\n");
            g_last_error = open_wifi_dialog();
            show_tester_screen();

            if (g_last_error < 0) {
                pspDebugScreenPrintf("\nWi-Fi dialog error: 0x%08X\n",
                    (unsigned int)g_last_error);
            }
        }

        previous_buttons = (int)buttons;
        sceKernelDelayThread(10000);
    }

    return 0;
}
