#include <pspkernel.h>
#include <pspdebug.h>

PSP_MODULE_INFO("PMSP Tester", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define DEMO_PRX_PATH "ms0:/PMSP/netdialog_demo.prx"

static volatile int g_running = 1;

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
        "PMSP Tester Exit Callback", exit_callback, NULL);

    if (callback_id >= 0)
        sceKernelRegisterExitCallback(callback_id);

    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    SceUID thread_id;

    thread_id = sceKernelCreateThread(
        "PMSP Tester Callback", callback_thread, 0x11, 0xFA0,
        PSP_THREAD_ATTR_USER, NULL);

    if (thread_id >= 0)
        sceKernelStartThread(thread_id, 0, NULL);
}

int main(void)
{
    SceUID module_id;
    int start_status = 0;
    int result;

    pspDebugScreenInit();
    setup_callbacks();

    pspDebugScreenPrintf("PMSP TESTER\n");
    pspDebugScreenPrintf("------------------------------\n");
    pspDebugScreenPrintf("Loading independent Netconf Demo PRX...\n");
    pspDebugScreenPrintf("Path: %s\n\n", DEMO_PRX_PATH);

    module_id = sceKernelLoadModule(DEMO_PRX_PATH, 0, NULL);
    if (module_id < 0) {
        pspDebugScreenPrintf("LOAD FAILED: 0x%08X\n",
                             (unsigned int)module_id);
    } else {
        pspDebugScreenPrintf("PRX loaded. Module ID: %d\n", module_id);
        result = sceKernelStartModule(
            module_id, 0, NULL, &start_status, NULL);

        if (result < 0) {
            pspDebugScreenPrintf("START FAILED: 0x%08X\n",
                                 (unsigned int)result);
        } else {
            pspDebugScreenPrintf("PRX start returned: 0x%08X\n",
                                 (unsigned int)result);
            pspDebugScreenPrintf("Start status: 0x%08X\n",
                                 (unsigned int)start_status);
        }
    }

    pspDebugScreenPrintf("\nThe demo PRX owns the Wi-Fi dialog and GU.\n");
    pspDebugScreenPrintf("Press HOME to exit.\n");

    while (g_running)
        sceKernelDelayThread(100000);

    sceKernelExitGame();
    return 0;
}
