/*
 * PMSP Wi-Fi Dialog Tester
 * Network dialog flow ported from the working minax2036-beep/psps main.c.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psputility.h>
#include <psputility_netconf.h>
#include <psputility_netmodules.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <psptypes.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("PMSP Wi-Fi Tester", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BUF_WIDTH 512
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define FRAME_SIZE (BUF_WIDTH * SCREEN_HEIGHT * 4)

static volatile int g_running = 1;
static const char *g_error_stage = "startup";
/* Match psps: allocate a 256 KiB GU command list. */
static unsigned int __attribute__((aligned(16))) g_display_list[65536];

static int g_common_loaded = 0;
static int g_inet_loaded = 0;
static int g_net_initialized = 0;
static int g_inet_initialized = 0;
static int g_apctl_initialized = 0;

struct Vertex {
    unsigned int color;
    float x;
    float y;
    float z;
};

static struct Vertex __attribute__((aligned(16))) g_vertices[12 * 3] = {
    {0xff7f0000, -1, -1,  1}, {0xff7f0000, -1,  1,  1}, {0xff7f0000,  1,  1,  1},
    {0xff7f0000, -1, -1,  1}, {0xff7f0000,  1,  1,  1}, {0xff7f0000,  1, -1,  1},
    {0xff7f0000, -1, -1, -1}, {0xff7f0000,  1, -1, -1}, {0xff7f0000,  1,  1, -1},
    {0xff7f0000, -1, -1, -1}, {0xff7f0000,  1,  1, -1}, {0xff7f0000, -1,  1, -1},

    {0xff007f00,  1, -1, -1}, {0xff007f00,  1, -1,  1}, {0xff007f00,  1,  1,  1},
    {0xff007f00,  1, -1, -1}, {0xff007f00,  1,  1,  1}, {0xff007f00,  1,  1, -1},
    {0xff007f00, -1, -1, -1}, {0xff007f00, -1,  1, -1}, {0xff007f00, -1,  1,  1},
    {0xff007f00, -1, -1, -1}, {0xff007f00, -1,  1,  1}, {0xff007f00, -1, -1,  1},

    {0xff00007f, -1,  1, -1}, {0xff00007f,  1,  1, -1}, {0xff00007f,  1,  1,  1},
    {0xff00007f, -1,  1, -1}, {0xff00007f,  1,  1,  1}, {0xff00007f, -1,  1,  1},
    {0xff00007f, -1, -1, -1}, {0xff00007f, -1, -1,  1}, {0xff00007f,  1, -1,  1},
    {0xff00007f, -1, -1, -1}, {0xff00007f,  1, -1,  1}, {0xff00007f,  1, -1, -1}
};

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
    SceUID callback_id;

    (void)args;
    (void)argp;

    callback_id = sceKernelCreateCallback(
        "PMSP Exit Callback", exit_callback, NULL);

    if (callback_id >= 0)
        sceKernelRegisterExitCallback(callback_id);

    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void)
{
    SceUID thread_id;

    thread_id = sceKernelCreateThread(
        "PMSP Callback Thread", callback_thread, 0x11, 0xFA0,
        PSP_THREAD_ATTR_USER, NULL);

    if (thread_id < 0)
        return (int)thread_id;

    return sceKernelStartThread(thread_id, 0, NULL);
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
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/*
 * Keep the initialization order and Apctl parameters from the working
 * psps project: COMMON -> INET -> sceNetInit -> InetInit -> ApctlInit.
 */
static int initialize_network(void)
{
    int result;

    g_error_stage = "sceUtilityLoadNetModule(COMMON)";
    result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    if (result < 0)
        return result;
    g_common_loaded = 1;

    g_error_stage = "sceUtilityLoadNetModule(INET)";
    result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    if (result < 0)
        return result;
    g_inet_loaded = 1;

    g_error_stage = "sceNetInit";
    result = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
    if (result < 0)
        return result;
    g_net_initialized = 1;

    g_error_stage = "sceNetInetInit";
    result = sceNetInetInit();
    if (result < 0)
        return result;
    g_inet_initialized = 1;

    g_error_stage = "sceNetApctlInit";
    result = sceNetApctlInit(0x8000, 48);
    if (result < 0)
        return result;
    g_apctl_initialized = 1;

    return 0;
}

static void terminate_network(void)
{
    if (g_apctl_initialized) {
        sceNetApctlTerm();
        g_apctl_initialized = 0;
    }

    if (g_inet_initialized) {
        sceNetInetTerm();
        g_inet_initialized = 0;
    }

    if (g_net_initialized) {
        sceNetTerm();
        g_net_initialized = 0;
    }

    if (g_inet_loaded) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        g_inet_loaded = 0;
    }

    if (g_common_loaded) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        g_common_loaded = 0;
    }
}

static void draw_background(void)
{
    sceGuStart(GU_DIRECT, g_display_list);
    sceGuClearColor(0xff222222);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
}

/* This is the dialog flow from the working psps/main.c. */
static int show_wifi_dialog(void)
{
    int result;
    int done = 0;
    pspUtilityNetconfData data;

    memset(&data, 0, sizeof(data));

    data.base.size = sizeof(data);
    sceUtilityGetSystemParamInt(
        PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &data.base.language);
    sceUtilityGetSystemParamInt(
        PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &data.base.buttonSwap);
    data.base.graphicsThread = 17;
    data.base.accessThread = 19;
    data.base.fontThread = 18;
    data.base.soundThread = 16;
    data.action = PSP_NETCONF_ACTION_CONNECTAP;

    g_error_stage = "sceUtilityNetconfInitStart";
    result = sceUtilityNetconfInitStart(&data);
    if (result < 0)
        return result;

    while (g_running && !done) {
        draw_background();

        switch (sceUtilityNetconfGetStatus()) {
        case PSP_UTILITY_DIALOG_VISIBLE:
            sceUtilityNetconfUpdate(1);
            break;

        case PSP_UTILITY_DIALOG_QUIT:
            sceUtilityNetconfShutdownStart();
            break;

        case PSP_UTILITY_DIALOG_NONE:
            done = 1;
            break;

        default:
            break;
        }

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}

int main(void)
{
    int result;
    int state = PSP_NET_APCTL_STATE_DISCONNECTED;

    /*
     * Show the known-working psps Netconf dialog before any PMSP UI or
     * application-specific processing:
     * callbacks -> GU -> network stack -> Netconf dialog -> PMSP code.
     *
     * Do not initialize pspDebugScreen before the dialog; its framebuffer
     * output is not needed during the native utility dialog.
     */
    g_error_stage = "setup_callbacks";
    result = setup_callbacks();
    if (result < 0)
        goto error;

    setup_gu();

    result = initialize_network();
    if (result < 0)
        goto error;

    pspDebugScreenPrintf("Network initialized.\n");
    pspDebugScreenPrintf("Opening Wi-Fi settings...\n");
    sceKernelDelayThread(500000);

    result = show_wifi_dialog();
    if (result < 0)
        goto error;

    g_error_stage = "sceNetApctlGetState";
    result = sceNetApctlGetState(&state);
    if (result < 0)
        goto error;

    terminate_network();
    sceGuTerm();

    pspDebugScreenInit();
    if (state == PSP_NET_APCTL_STATE_GOT_IP)
        pspDebugScreenPrintf("Connected: IP address acquired.\n");
    else
        pspDebugScreenPrintf("Dialog closed. Not connected.\n");

    pspDebugScreenPrintf("Press HOME to exit.\n");
    while (g_running)
        sceKernelDelayThread(100000);

    sceKernelExitGame();
    return 0;

error:
    terminate_network();
    sceGuTerm();
    pspDebugScreenInit();
    pspDebugScreenPrintf("PMSP Wi-Fi Tester failed\n");
    pspDebugScreenPrintf("Stage: %s\n", g_error_stage);
    pspDebugScreenPrintf("Error: 0x%08X\n", (unsigned int)result);
    pspDebugScreenPrintf("\nPress HOME to exit.\n");

    while (g_running)
        sceKernelDelayThread(100000);

    sceKernelExitGame();
    return 0;
}
