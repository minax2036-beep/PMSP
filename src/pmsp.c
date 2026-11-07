#include "pmsp.h"
#include "platform/posix/net.h"
#include <stdio.h>
#include <string.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#define PMSP_DEFAULT_PORT "443"
#define PMSP_DEFAULT_CA_FILE "/etc/ssl/certs/ca-certificates.crt"
#define PMSP_IO_BUFFER_SIZE 4096
#define PMSP_REQUEST_BUFFER_SIZE 2048

static int has_line_break(const char *s)
{
    return s != NULL && (strchr(s, '\r') != NULL || strchr(s, '\n') != NULL);
}

static void report_tls_error(const char *operation, int code)
{
    char text[256];
    mbedtls_strerror(code, text, sizeof(text));
    fprintf(stderr, "PMSP: %s failed: %s (-0x%04X)\n",
            operation, text, (unsigned int)(-code));
}

int pmsp_https_get(const char *host, const char *port, const char *path,
                   const char *ca_file, pmsp_write_callback callback,
                   void *context)
{
    int result = PMSP_ERR_TLS;
    int fd = -1;
    int ret;
    int verify_flags;
    size_t request_len;
    size_t offset;
    unsigned char buffer[PMSP_IO_BUFFER_SIZE];
    char request[PMSP_REQUEST_BUFFER_SIZE];
    char verify_info[512];
    const char *effective_port = port ? port : PMSP_DEFAULT_PORT;
    const char *effective_ca = ca_file ? ca_file : PMSP_DEFAULT_CA_FILE;
    const char *personalization = "PMSP HTTPS client";

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt ca;

    if (!host || !host[0] || !path || path[0] != '/' || !callback ||
        has_line_break(host) || has_line_break(effective_port) ||
        has_line_break(path)) return PMSP_ERR_ARGUMENT;

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_x509_crt_init(&ca);

    ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
        (const unsigned char *)personalization, strlen(personalization));
    if (ret != 0) {
        report_tls_error("random generator initialization", ret);
        goto cleanup;
    }

    ret = mbedtls_x509_crt_parse_file(&ca, effective_ca);
    if (ret < 0) {
        fprintf(stderr, "PMSP: cannot load CA bundle: %s\n", effective_ca);
        report_tls_error("CA bundle loading", ret);
        result = PMSP_ERR_CA_FILE;
        goto cleanup;
    }

    ret = mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        report_tls_error("TLS configuration", ret);
        goto cleanup;
    }

    mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&config, &ca, NULL);
    mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);

    ret = mbedtls_ssl_setup(&ssl, &config);
    if (ret != 0) {
        report_tls_error("TLS setup", ret);
        goto cleanup;
    }

    ret = mbedtls_ssl_set_hostname(&ssl, host);
    if (ret != 0) {
        report_tls_error("hostname setup", ret);
        goto cleanup;
    }

    fprintf(stderr, "PMSP: connecting to %s:%s...\n", host, effective_port);
    fd = pmsp_net_connect(host, effective_port);
    if (fd < 0) {
        fprintf(stderr, "PMSP: DNS lookup or TCP connection failed.\n");
        result = PMSP_ERR_CONNECT;
        goto cleanup;
    }

    mbedtls_ssl_set_bio(&ssl, &fd, pmsp_net_send, pmsp_net_recv, NULL);
    fprintf(stderr, "PMSP: TLS handshake...\n");
    do {
        ret = mbedtls_ssl_handshake(&ssl);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret != 0) {
        report_tls_error("TLS handshake", ret);
        goto cleanup;
    }

    verify_flags = (int)mbedtls_ssl_get_verify_result(&ssl);
    if (verify_flags != 0) {
        mbedtls_x509_crt_verify_info(verify_info, sizeof(verify_info),
                                     "  ! ", (uint32_t)verify_flags);
        fprintf(stderr, "PMSP: certificate verification failed:\n%s", verify_info);
        result = PMSP_ERR_VERIFY;
        goto cleanup;
    }

    ret = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: PMSP/%s\r\n"
        "Accept: */*\r\nConnection: close\r\n\r\n",
        path, host, PMSP_VERSION);
    if (ret < 0 || (size_t)ret >= sizeof(request)) {
        fprintf(stderr, "PMSP: HTTP request is too large.\n");
        result = PMSP_ERR_ARGUMENT;
        goto cleanup;
    }

    request_len = (size_t)ret;
    offset = 0;
    while (offset < request_len) {
        ret = mbedtls_ssl_write(&ssl,
            (const unsigned char *)request + offset, request_len - offset);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret <= 0) {
            report_tls_error("HTTP request write", ret);
            goto cleanup;
        }
        offset += (size_t)ret;
    }

    fprintf(stderr, "PMSP: receiving HTTP response...\n");
    for (;;) {
        ret = mbedtls_ssl_read(&ssl, buffer, sizeof(buffer));
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) break;
        if (ret < 0) {
            report_tls_error("HTTP response read", ret);
            goto cleanup;
        }
        if (callback(context, buffer, (size_t)ret) != 0) {
            fprintf(stderr, "PMSP: response callback failed.\n");
            result = PMSP_ERR_OUTPUT;
            goto cleanup;
        }
    }

    result = PMSP_OK;

cleanup:
    if (fd >= 0) (void)mbedtls_ssl_close_notify(&ssl);
    pmsp_net_close(fd);
    mbedtls_x509_crt_free(&ca);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_ssl_config_free(&config);
    mbedtls_ssl_free(&ssl);
    return result;
}
