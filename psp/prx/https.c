#include <string.h>
#include <pspkernel.h>
#include <psphttp.h>
#include <pspiofilemgr.h>

#include "pmsp_api.h"

#define PMSP_HTTP_BUFFER_SIZE 4096
#define PMSP_PATH_MAX 240

extern int g_last_error;

static int is_https_url(const char *url)
{
    static const char prefix[] = "https://";
    unsigned int i;

    if (url == NULL) {
        return 0;
    }

    for (i = 0; i < sizeof(prefix) - 1; ++i) {
        if (url[i] != prefix[i]) {
            return 0;
        }
    }

    if (url[sizeof(prefix) - 1] == '\0') {
        return 0;
    }

    return 1;
}

int pmspHttpsGet(const char *url, const char *output_path)
{
    int http_initialized = 0;
    int https_initialized = 0;
    int template_id = -1;
    int connection_id = -1;
    int request_id = -1;
    SceUID file = -1;
    char temporary_path[PMSP_PATH_MAX + 5];
    unsigned char buffer[PMSP_HTTP_BUFFER_SIZE];
    int result = PMSP_ERROR_NETWORK;
    int status_code = 0;
    int read_size;
    int write_size;
    unsigned int path_length;

    if (!is_https_url(url) || output_path == NULL || output_path[0] == '\0') {
        g_last_error = PMSP_ERROR_INVALID_ARG;
        return PMSP_ERROR_INVALID_ARG;
    }

    path_length = (unsigned int)strlen(output_path);
    if (path_length == 0 || path_length > PMSP_PATH_MAX) {
        g_last_error = PMSP_ERROR_INVALID_ARG;
        return PMSP_ERROR_INVALID_ARG;
    }

    memcpy(temporary_path, output_path, path_length);
    memcpy(temporary_path + path_length, ".tmp", 5);

    /*
     * The calling Homebrew must have initialized the PSP network stack
     * and connected to an access point before calling this function.
     */
    if (sceHttpInit(20000) < 0) {
        result = PMSP_ERROR_NETWORK;
        goto cleanup;
    }
    http_initialized = 1;

    if (sceHttpsInit(0, 0, 0, 0) < 0) {
        result = PMSP_ERROR_TLS;
        goto cleanup;
    }
    https_initialized = 1;

    if (sceHttpsLoadDefaultCert(0, 0) < 0) {
        result = PMSP_ERROR_TLS;
        goto cleanup;
    }

    template_id = sceHttpCreateTemplate("PMSP/0.2", 1, 0);
    if (template_id < 0) {
        result = PMSP_ERROR_NETWORK;
        goto cleanup;
    }

    /*
     * Redirects are deliberately disabled. This prevents an HTTPS request
     * from silently following a redirect to an unencrypted HTTP URL.
     */
    if (sceHttpDisableRedirect(template_id) < 0) {
        result = PMSP_ERROR_NETWORK;
        goto cleanup;
    }

    if (sceHttpSetConnectTimeOut(template_id, 30000000) < 0 ||
        sceHttpSetRecvTimeOut(template_id, 30000000) < 0) {
        result = PMSP_ERROR_NETWORK;
        goto cleanup;
    }

    connection_id = sceHttpCreateConnectionWithURL(
        template_id, url, 0
    );
    if (connection_id < 0) {
        result = PMSP_ERROR_NETWORK;
        goto cleanup;
    }

    request_id = sceHttpCreateRequestWithURL(
        connection_id, PSP_HTTP_METHOD_GET, (char *)url, 0
    );
    if (request_id < 0) {
        result = PMSP_ERROR_NETWORK;
        goto cleanup;
    }

    if (sceHttpSendRequest(request_id, NULL, 0) < 0) {
        result = PMSP_ERROR_NETWORK;
        goto cleanup;
    }

    if (sceHttpGetStatusCode(request_id, &status_code) < 0) {
        result = PMSP_ERROR_HTTP;
        goto cleanup;
    }

    if (status_code < 200 || status_code >= 300) {
        result = PMSP_ERROR_HTTP;
        goto cleanup;
    }

    file = sceIoOpen(
        temporary_path,
        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC,
        0777
    );
    if (file < 0) {
        result = PMSP_ERROR_IO;
        goto cleanup;
    }

    for (;;) {
        read_size = sceHttpReadData(
            request_id, buffer, sizeof(buffer)
        );

        if (read_size < 0) {
            result = PMSP_ERROR_NETWORK;
            goto cleanup;
        }

        if (read_size == 0) {
            break;
        }

        write_size = sceIoWrite(file, buffer, (SceSize)read_size);
        if (write_size != read_size) {
            result = PMSP_ERROR_IO;
            goto cleanup;
        }
    }

    if (sceIoClose(file) < 0) {
        file = -1;
        result = PMSP_ERROR_IO;
        goto cleanup;
    }
    file = -1;

    if (sceIoRename(temporary_path, output_path) < 0) {
        result = PMSP_ERROR_IO;
        goto cleanup;
    }

    result = PMSP_OK;

cleanup:
    if (file >= 0) {
        sceIoClose(file);
    }

    if (result != PMSP_OK) {
        sceIoRemove(temporary_path);
    }

    if (request_id >= 0) {
        sceHttpDeleteRequest(request_id);
    }
    if (connection_id >= 0) {
        sceHttpDeleteConnection(connection_id);
    }
    if (template_id >= 0) {
        sceHttpDeleteTemplate(template_id);
    }
    if (https_initialized) {
        sceHttpsEnd();
    }
    if (http_initialized) {
        sceHttpEnd();
    }

    g_last_error = result;
    return result;
}
