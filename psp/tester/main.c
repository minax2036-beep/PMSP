#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psputility.h>
#include <psputility_netconf.h>
#include <psputility_netmodules.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <string.h>

PSP_MODULE_INFO("PMSP Tester", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define BUF_WIDTH 512
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define FRAME_SIZE (BUF_WIDTH * SCREEN_HEIGHT * 4)

/* The official sample uses a much larger list; this screen only clears. */
static unsigned int __attribute__((aligned(16))) g_display_list[4096];
static int g_running = 1;
static int g_net_initialized;
static int g_common_loaded;
static int g_inet_loaded;

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;
    g_running = 0;
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int callback_id;
    (void)args;
    (void)argp;

    callback_id = sceKernelCreateCallback("PMSP Exit Callback",
                                           exit_callback, NULL);
    if (callback_id >= 0)
        sceKernelRegisterExitCallback(callback_id);

    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    SceUID thread_id = sceKernelCreateThread(
        "PMSP Callback Thread", callback_thread, 0x11, 0xFA0,
        PSP_THREAD_ATTR_USER, NULL);

    if (thread_id >= 0)
        sceKernelStartThread(thread_id, 0, NULL);
}

static void setup_gu(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, g_display_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT,
                    (void *)FRAME_SIZE, BUF_WIDTH);
    sceGuDepthBuffer((void *)(FRAME_SIZE * 2), BUF_WIDTH);
    sceGuOffset(2048 - (SCREEN_WIDTH / 2),
                2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(0xc350, 0x2710);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void draw_frame(void)
{
    sceGuStart(GU_DIRECT, g_display_list);
    sceGuClearColor(0xff000000);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static int load_network_modules(void)
{
    int result;

    if (!g_common_loaded) {
        result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
        if (result < 0)
            return result;
        g_common_loaded = 1;
    }

    if (!g_inet_loaded) {
        result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
        if (result < 0)
            return result;
        g_inet_loaded = 1;
    }

    return 0;
}

/* Same initialization sequence used by the official PSPSDK netdialog sample. */
static int net_init(void)
{
    int result;

    result = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
    if (result < 0)
        return result;

    result = sceNetInetInit();
    if (result < 0) {
        sceNetTerm();
        return result;
    }

    result = sceNetApctlInit(0x8000, 48);
    if (result < 0) {
        sceNetInetTerm();
        sceNetTerm();
        return result;
    }

    g_net_initialized = 1;
    return 0;
}

static void net_term(void)
{
    if (!g_net_initialized)
        return;

    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
    g_net_initialized = 0;
}

/* Based on PSPSDK src/samples/utility/netdialog/main.c. */
static int open_wifi_dialog(void)
{
    int result;
    int done = 0;
    pspUtilityNetconfData data;
    struct pspUtilityNetconfAdhoc adhocparam;

    memset(&data, 0, sizeof(data));
    data.base.size = sizeof(data);
    data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    data.base.graphicsThread = 17;
    data.base.accessThread = 19;
    data.base.fontThread = 18;
    data.base.soundThread = 16;
    data.action = PSP_NETCONF_ACTION_CONNECTAP;

    memset(&adhocparam, 0, sizeof(adhocparam));
    data.adhocparam = &adhocparam;

    result = sceUtilityNetconfInitStart(&data);
    if (result < 0)
        return result;

    while (g_running && !done) {
        draw_frame();

        switch (sceUtilityNetconfGetStatus()) {
        case PSP_UTILITY_DIALOG_NONE:
            break;

        case PSP_UTILITY_DIALOG_VISIBLE:
            sceUtilityNetconfUpdate(1);
            break;

        case PSP_UTILITY_DIALOG_QUIT:
            sceUtilityNetconfShutdownStart();
            break;

        case PSP_UTILITY_DIALOG_FINISHED:
            done = 1;
            break;

        default:
            break;
        }
    }

    return 0;
}

static void show_tester_screen(int last_error)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("PMSP TESTER\n");
    pspDebugScreenPrintf("------------------------------\n\n");
    pspDebugScreenPrintf("Official PSPSDK Netconf flow\n\n");
    pspDebugScreenPrintf("CROSS: Open Wi-Fi settings again\n");
    pspDebugScreenPrintf("HOME: PSP system exit menu\n");

    if (last_error < 0)
        pspDebugScreenPrintf("\nError: 0x%08X\n", (unsigned int)last_error);
}

int main(void)
{
    SceCtrlData pad;
    unsigned int previous_buttons = 0;
    int result;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenInit();
    setup_callbacks();

    pspDebugScreenPrintf("PMSP TESTER\n");
    pspDebugScreenPrintf("Initializing official Wi-Fi dialog flow...\n");

    result = load_network_modules();
    if (result >= 0)
        result = net_init();

    if (result >= 0) {
        setup_gu();
        result = open_wifi_dialog();
    }

    show_tester_screen(result);

    while (g_running) {
        unsigned int buttons;

        sceCtrlReadBufferPositive(&pad, 1);
        buttons = pad.Buttons;

        if ((buttons & PSP_CTRL_CROSS) &&
            !(previous_buttons & PSP_CTRL_CROSS)) {
            pspDebugScreenPrintf("\nOpening Wi-Fi settings...\n");
            result = open_wifi_dialog();
            show_tester_screen(result);
        }

        previous_buttons = buttons;
        sceKernelDelayThread(10000);
    }

    net_term();
    sceKernelExitGame();
    return 0;
}
