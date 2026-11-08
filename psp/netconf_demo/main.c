/*
 * PMSP Netconf Demo PRX
 * Based on the PSPSDK utility/netdialog sample.
 * Original sample: https://github.com/pspdev/pspsdk/tree/master/src/samples/utility/netdialog
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

PSP_MODULE_INFO("PMSP Netconf Demo", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

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

static int g_exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;
    g_running = 0;
    return 0;
}

static int g_callback_thread(SceSize args, void *argp)
{
    SceUID callback_id;

    (void)args;
    (void)argp;

    callback_id = sceKernelCreateCallback(
        "PMSP Netconf Exit Callback", g_exit_callback, NULL);

    if (callback_id >= 0)
        sceKernelRegisterExitCallback(callback_id);

    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void)
{
    SceUID thread_id;

    thread_id = sceKernelCreateThread(
        "PMSP Netconf Callback", g_callback_thread, 0x11, 0xFA0,
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
    static int value = 0;
    ScePspFVector3 position = {0.0f, 0.0f, -5.0f};
    ScePspFVector3 rotation = {
        value * 0.79f * ((float)M_PI / 180.0f),
        value * 0.98f * ((float)M_PI / 180.0f),
        value * 1.32f * ((float)M_PI / 180.0f)
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
    value++;
}

static int initialize_network(void)
{
    int result;

    g_error_stage = "Load COMMON network module";
    result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    if (result < 0)
        return result;

    g_error_stage = "Load INET network module";
    result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    if (result < 0)
        return result;

    g_error_stage = "sceNetInit";
    result = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
    if (result < 0)
        return result;

    g_error_stage = "sceNetInetInit";
    result = sceNetInetInit();
    if (result < 0) {
        sceNetTerm();
        return result;
    }

    g_error_stage = "sceNetApctlInit";
    result = sceNetApctlInit(0x8000, 48);
    if (result < 0) {
        sceNetInetTerm();
        sceNetTerm();
        return result;
    }

    return 0;
}

static void terminate_network(void)
{
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
}

static int run_netconf_dialog(void)
{
    int result;
    int done = 0;
    pspUtilityNetconfData data;
    struct pspUtilityNetconfAdhoc adhoc_parameters;

    memset(&data, 0, sizeof(data));
    data.base.size = sizeof(data);
    data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
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
        draw_demo_frame();

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

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}

static int demo_thread(SceSize args, void *argp)
{
    int result;

    (void)args;
    (void)argp;

    g_error_stage = "setup_callbacks";
    result = setup_callbacks();
    if (result < 0)
        goto finished;

    result = initialize_network();
    if (result < 0)
        goto finished;

    setup_gu();
    result = run_netconf_dialog();
    terminate_network();

finished:
    if (result < 0) {
        pspDebugScreenInit();
        pspDebugScreenPrintf("PMSP Netconf Demo failed\\n");
        pspDebugScreenPrintf("Stage: %s\\n", g_error_stage);
        pspDebugScreenPrintf("Error: 0x%08X\\n", (unsigned int)result);
    }
    sceKernelExitDeleteThread(result);
    return 0;
}

int module_start(SceSize args, void *argp)
{
    SceUID thread_id;

    (void)args;
    (void)argp;

    g_running = 1;

    thread_id = sceKernelCreateThread(
        "PMSP Netconf Demo Thread", demo_thread, 0x18, 0x10000,
        PSP_THREAD_ATTR_USER, NULL);

    if (thread_id < 0)
        return (int)thread_id;

    return sceKernelStartThread(thread_id, 0, NULL);
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    g_running = 0;
    return 0;
}
