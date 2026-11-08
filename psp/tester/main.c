/*
 * PMSP Wi-Fi Dialog Tester
 * The Netconf dialog now runs directly inside this EBOOT.PBP.
 * Network initialization follows the official PSPSDK netdialog sample.
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

PSP_MODULE_INFO("PMSP Wi-Fi Tester", PSP_MODULE_USER, 1, 0);
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
static unsigned int __attribute__((aligned(16))) g_display_list[4096];

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
    result = sceNetApctlInit(0x2000, 42);
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

static void draw_demo_frame(void)
{
    static int frame = 0;
    ScePspFVector3 position = {0.0f, 0.0f, -5.0f};
    ScePspFVector3 rotation = {
        frame * 0.79f * ((float)M_PI / 180.0f),
        frame * 0.98f * ((float)M_PI / 180.0f),
        frame * 1.32f * ((float)M_PI / 180.0f)
    };

    sceGuStart(GU_DIRECT, g_display_list);
    sceGuClearColor(0xff554433);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(75.0f, 16.0f / 9.0f, 0.5f, 1000.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumTranslate(&position);
    sceGumRotateXYZ(&rotation);

    sceGumDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        12 * 3, NULL, g_vertices);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    frame++;
}

static int show_wifi_dialog(void)
{
    int result;
    int status;
    int shutdown_started = 0;
    int done = 0;
    pspUtilityNetconfData data;
    struct pspUtilityNetconfAdhoc adhoc_parameters;

    memset(&data, 0, sizeof(data));
    data.base.size = sizeof(data);
    data.base.language = PSP_SYSTEMPARAM_LANGUAGE_JAPANESE;
    data.base.buttonSwap = PSP_UTILITY_ACCEPT_CIRCLE;
    data.base.graphicsThread = 17;
    data.base.accessThread = 19;
    data.base.fontThread = 18;
    data.base.soundThread = 16;
    data.action = PSP_NETCONF_ACTION_CONNECTAP;

    memset(&adhoc_parameters, 0, sizeof(adhoc_parameters));
    data.adhocparam = &adhoc_parameters;

    g_error_stage = "sceUtilityNetconfInitStart";
    result = sceUtilityNetconfInitStart(&data);
    if (result < 0)
        return result;

    while (g_running && !done) {
        status = sceUtilityNetconfGetStatus();

        switch (status) {
        case PSP_UTILITY_DIALOG_VISIBLE:
            draw_demo_frame();
            sceUtilityNetconfUpdate(1);
            sceDisplayWaitVblankStart();
            sceGuSwapBuffers();
            break;

        case PSP_UTILITY_DIALOG_QUIT:
            if (!shutdown_started) {
                sceUtilityNetconfShutdownStart();
                shutdown_started = 1;
            }
            sceDisplayWaitVblankStart();
            break;

        case PSP_UTILITY_DIALOG_FINISHED:
            sceDisplayWaitVblankStart();
            break;

        case PSP_UTILITY_DIALOG_NONE:
            done = 1;
            break;

        case PSP_UTILITY_DIALOG_INIT:
        default:
            sceDisplayWaitVblankStart();
            break;
        }
    }

    return 0;
}

int main(void)
{
    int result;
    int state = PSP_NET_APCTL_STATE_DISCONNECTED;

    pspDebugScreenInit();

    pspDebugScreenPrintf("PMSP WI-FI TESTER\n");
    pspDebugScreenPrintf("------------------------------\n");
    pspDebugScreenPrintf("Dialog is integrated into EBOOT.PBP.\n");
    pspDebugScreenPrintf("Initializing network modules...\n");

    result = setup_callbacks();
    if (result < 0) {
        g_error_stage = "setup_callbacks";
        goto error;
    }

    result = initialize_network();
    if (result < 0)
        goto error;

    pspDebugScreenPrintf("Network initialized.\n");
    pspDebugScreenPrintf("Opening Wi-Fi settings...\n");
    sceKernelDelayThread(500000);

    setup_gu();
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
