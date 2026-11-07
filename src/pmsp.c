#include "pmsp.h"
#include "platform/posix/net.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#define PMSP_DEFAULT_PORT "443"
#define PMSP_DEFAULT_CA_FILE "/etc/ssl/certs/ca-certificates.crt"
#define PMSP_IO_BUFFER_SIZE 4096
#define PMSP_REQUEST_BUFFER_SIZE 2048
#define PMSP_HTTP_LINE_SIZE 8192
#define PMSP_HTTP_HEADER_LIMIT 16384

typedef struct {
    mbedtls_ssl_context *ssl;
    unsigned char buffer[PMSP_IO_BUFFER_SIZE];
    size_t position;
    size_t length;
    int eof;
} pmsp_reader;

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

static int reader_fill(pmsp_reader *reader)
{
    int ret;

    if (reader->position < reader->length) return 1;
    if (reader->eof) return 0;

    reader->position = 0;
    reader->length = 0;
    for (;;) {
        ret = mbedtls_ssl_read(reader->ssl, reader->buffer,
                               sizeof(reader->buffer));
        if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
            ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            reader->eof = 1;
            return 0;
        }
        if (ret < 0) {
            report_tls_error("HTTP response read", ret);
            return -1;
        }
        reader->length = (size_t)ret;
        return 1;
    }
}

static int reader_get_byte(pmsp_reader *reader, unsigned char *value)
{
    int available = reader_fill(reader);
    if (available <= 0) return available;
    *value = reader->buffer[reader->position++];
    return 1;
}

static int reader_read_line(pmsp_reader *reader, char *line, size_t capacity,
                            size_t *line_length)
{
    size_t used = 0;
    unsigned char ch;
    int result;

    if (capacity < 2) return -1;
    for (;;) {
        result = reader_get_byte(reader, &ch);
        if (result <= 0) return -1;
        if (ch == '\n') {
            if (used == 0 || line[used - 1] != '\r') return -1;
            --used;
            line[used] = '\0';
            if (line_length) *line_length = used;
            return 0;
        }
        if (used + 1 >= capacity) return -1;
        line[used++] = (char)ch;
    }
}

static int reader_read_exact(pmsp_reader *reader, uint64_t amount,
                             pmsp_write_callback callback, void *context)
{
    while (amount > 0) {
        size_t available;
        size_t take;
        int result = reader_fill(reader);

        if (result <= 0) return -1;
        available = reader->length - reader->position;
        take = available;
        if ((uint64_t)take > amount) take = (size_t)amount;
        if (callback(context, reader->buffer + reader->position, take) != 0)
            return -2;
        reader->position += take;
        amount -= (uint64_t)take;
    }
    return 0;
}

static int reader_copy_to_eof(pmsp_reader *reader,
                              pmsp_write_callback callback, void *context)
{
    for (;;) {
        int result = reader_fill(reader);
        size_t available;

        if (result == 0) return 0;
        if (result < 0) return -1;
        available = reader->length - reader->position;
        if (callback(context, reader->buffer + reader->position, available) != 0)
            return -2;
        reader->position += available;
    }
}

static char *trim_whitespace(char *text)
{
    char *end;
    while (*text && isspace((unsigned char)*text)) ++text;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    return text;
}

static int header_has_chunked_token(const char *value)
{
    const char *cursor = value;

    while (*cursor) {
        const char *start;
        size_t length;
        while (*cursor == ',' || isspace((unsigned char)*cursor)) ++cursor;
        start = cursor;
        while (*cursor && *cursor != ',') ++cursor;
        length = (size_t)(cursor - start);
        while (length && isspace((unsigned char)start[length - 1])) --length;
        if (length == 7 && strncasecmp(start, "chunked", 7) == 0) return 1;
    }
    return 0;
}

static int parse_content_length(const char *value, uint64_t *length)
{
    char *end;
    unsigned long long parsed;

    while (isspace((unsigned char)*value)) ++value;
    if (!*value || *value == '-') return -1;
    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno == ERANGE || end == value) return -1;
    while (isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return -1;
    *length = (uint64_t)parsed;
    return 0;
}

static int read_chunked_body(pmsp_reader *reader,
                             pmsp_write_callback callback, void *context)
{
    char line[256];
    unsigned char a, b;
    uint64_t chunk_size;
    char *end;
    int result;

    for (;;) {
        size_t line_length;
        result = reader_read_line(reader, line, sizeof(line), &line_length);
        if (result != 0 || line_length == 0) return -1;

        errno = 0;
        chunk_size = strtoull(line, &end, 16);
        if (errno == ERANGE || end == line) return -1;
        if (*end != '\0' && *end != ';') return -1;

        if (chunk_size == 0) {
            do {
                result = reader_read_line(reader, line, sizeof(line), NULL);
                if (result != 0) return -1;
            } while (line[0] != '\0');
            return 0;
        }

        result = reader_read_exact(reader, chunk_size, callback, context);
        if (result != 0) return result;
        if (reader_get_byte(reader, &a) != 1 ||
            reader_get_byte(reader, &b) != 1 || a != '\r' || b != '\n')
            return -1;
    }
}

static int pmsp_https_get_once(const char *host, const char *port, const char *path,
                              const char *ca_file, pmsp_write_callback callback,
                              void *context, char *redirect_location,
                              size_t redirect_capacity)
{
    int result = PMSP_ERR_TLS;
    int fd = -1;
    int ret;
    int verify_flags;
    int status_code = 0;
    int is_redirect = 0;
    int has_location = 0;
    int chunked = 0;
    int has_content_length = 0;
    uint64_t content_length = 0;
    size_t request_len;
    size_t offset;
    size_t header_bytes = 0;
    char request[PMSP_REQUEST_BUFFER_SIZE];
    char verify_info[512];
    char line[PMSP_HTTP_LINE_SIZE];
    const char *effective_port = port ? port : PMSP_DEFAULT_PORT;
    const char *effective_ca = ca_file ? ca_file : PMSP_DEFAULT_CA_FILE;
    const char *personalization = "PMSP HTTPS client";
    pmsp_reader reader;

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt ca;

    if (!host || !host[0] || !path || path[0] != '/' || !callback ||
        has_line_break(host) || has_line_break(effective_port) ||
        has_line_break(path)) return PMSP_ERR_ARGUMENT;

    memset(&reader, 0, sizeof(reader));
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
        "Accept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
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

    reader.ssl = &ssl;
    fprintf(stderr, "PMSP: reading HTTP headers...\n");

    ret = reader_read_line(&reader, line, sizeof(line), NULL);
    if (ret != 0 || sscanf(line, "HTTP/%*u.%*u %d", &status_code) != 1 ||
        status_code < 100 || status_code > 599) {
        fprintf(stderr, "PMSP: invalid HTTP status line.\n");
        result = PMSP_ERR_HTTP;
        goto cleanup;
    }

    for (;;) {
        char *colon;
        char *name;
        char *value;
        size_t line_length;

        ret = reader_read_line(&reader, line, sizeof(line), &line_length);
        if (ret != 0) {
            fprintf(stderr, "PMSP: invalid or incomplete HTTP headers.\n");
            result = PMSP_ERR_HTTP;
            goto cleanup;
        }
        header_bytes += line_length + 2;
        if (header_bytes > PMSP_HTTP_HEADER_LIMIT) {
            fprintf(stderr, "PMSP: HTTP headers exceed the configured limit.\n");
            result = PMSP_ERR_HTTP;
            goto cleanup;
        }
        if (line_length == 0) break;

        colon = strchr(line, ':');
        if (!colon) {
            fprintf(stderr, "PMSP: malformed HTTP header.\n");
            result = PMSP_ERR_HTTP;
            goto cleanup;
        }
        *colon = '\0';
        name = trim_whitespace(line);
        value = trim_whitespace(colon + 1);

        if (strcasecmp(name, "Transfer-Encoding") == 0) {
            if (header_has_chunked_token(value)) chunked = 1;
        } else if (strcasecmp(name, "Location") == 0) {
            size_t location_length = strlen(value);
            if (location_length == 0 || location_length >= redirect_capacity) {
                fprintf(stderr, "PMSP: invalid or oversized redirect Location.\n");
                result = PMSP_ERR_HTTP;
                goto cleanup;
            }
            memcpy(redirect_location, value, location_length + 1);
            has_location = 1;
        } else if (strcasecmp(name, "Content-Length") == 0) {
            uint64_t parsed_length;
            if (parse_content_length(value, &parsed_length) != 0 ||
                (has_content_length && parsed_length != content_length)) {
                fprintf(stderr, "PMSP: invalid or conflicting Content-Length.\n");
                result = PMSP_ERR_HTTP;
                goto cleanup;
            }
            content_length = parsed_length;
            has_content_length = 1;
        }
    }

    is_redirect = (status_code == 301 || status_code == 302 || status_code == 303 ||
                   status_code == 307 || status_code == 308);
    if (is_redirect) {
        if (!has_location) {
            fprintf(stderr, "PMSP: redirect response has no Location header.\n");
            result = PMSP_ERR_HTTP;
            goto cleanup;
        }
        fprintf(stderr, "PMSP: HTTP redirect %d -> %s\n", status_code, redirect_location);
        result = 1; /* Internal signal: follow redirect. */
        goto cleanup;
    }

    fprintf(stderr, "PMSP: HTTP status %d; receiving response body...\n", status_code);
    if (chunked) {
        ret = read_chunked_body(&reader, callback, context);
    } else if (has_content_length) {
        ret = reader_read_exact(&reader, content_length, callback, context);
    } else {
        ret = reader_copy_to_eof(&reader, callback, context);
    }

    if (ret == -2) {
        fprintf(stderr, "PMSP: response callback failed.\n");
        result = PMSP_ERR_OUTPUT;
        goto cleanup;
    }
    if (ret != 0) {
        fprintf(stderr, "PMSP: malformed or incomplete HTTP response body.\n");
        result = PMSP_ERR_HTTP;
        goto cleanup;
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


#define PMSP_MAX_REDIRECTS 5
#define PMSP_REDIRECT_HOST_SIZE 256
#define PMSP_REDIRECT_PORT_SIZE 16
#define PMSP_REDIRECT_PATH_SIZE 4096
#define PMSP_REDIRECT_LOCATION_SIZE 4096

static int starts_with_case(const char *text, const char *prefix)
{
    return strncasecmp(text, prefix, strlen(prefix)) == 0;
}

static int copy_url_part(char *destination, size_t capacity,
                         const char *source, size_t length)
{
    if (length == 0 || length >= capacity) return -1;
    memcpy(destination, source, length);
    destination[length] = '\0';
    return 0;
}

static int resolve_redirect(const char *current_host, const char *current_port,
                            const char *current_path, const char *location,
                            char *next_host, size_t host_capacity,
                            char *next_port, size_t port_capacity,
                            char *next_path, size_t path_capacity)
{
    const char *authority;
    const char *path_start;
    const char *authority_end;
    const char *fragment;
    const char *colon;
    size_t authority_length;
    size_t path_length;

    if (!location || !location[0] || has_line_break(location)) return -1;

    if (starts_with_case(location, "https://")) {
        authority = location + 8;
    } else if (starts_with_case(location, "http://")) {
        fprintf(stderr, "PMSP: refusing HTTPS-to-HTTP redirect.\n");
        return -1;
    } else if (strstr(location, "://") != NULL) {
        fprintf(stderr, "PMSP: unsupported redirect URL scheme.\n");
        return -1;
    } else if (location[0] == '/' && location[1] == '/') {
        authority = location + 2;
    } else {
        if (strlen(current_host) >= host_capacity ||
            strlen(current_port) >= port_capacity) return -1;
        strcpy(next_host, current_host);
        strcpy(next_port, current_port);

        fragment = strchr(location, '#');
        path_length = fragment ? (size_t)(fragment - location) : strlen(location);
        if (path_length == 0) return -1;

        if (location[0] == '/') {
            return copy_url_part(next_path, path_capacity, location, path_length);
        }

        if (location[0] == '?') {
            const char *query_base = strchr(current_path, '?');
            size_t base_length = query_base
                ? (size_t)(query_base - current_path) : strlen(current_path);
            if (base_length + path_length >= path_capacity) return -1;
            memcpy(next_path, current_path, base_length);
            memcpy(next_path + base_length, location, path_length);
            next_path[base_length + path_length] = '\0';
            return 0;
        }

        {
            const char *query = strchr(current_path, '?');
            size_t base_length = query
                ? (size_t)(query - current_path) : strlen(current_path);
            const char *last_slash = NULL;
            size_t directory_length;
            size_t i;

            for (i = 0; i < base_length; ++i)
                if (current_path[i] == '/') last_slash = current_path + i;
            directory_length = last_slash
                ? (size_t)(last_slash - current_path) + 1 : 1;
            if (directory_length + path_length >= path_capacity) return -1;
            memcpy(next_path, current_path, directory_length);
            memcpy(next_path + directory_length, location, path_length);
            next_path[directory_length + path_length] = '\0';
            return 0;
        }
    }

    authority_end = authority;
    while (*authority_end && *authority_end != '/' &&
           *authority_end != '?' && *authority_end != '#')
        ++authority_end;
    authority_length = (size_t)(authority_end - authority);
    if (authority_length == 0 || memchr(authority, '@', authority_length) != NULL ||
        memchr(authority, '[', authority_length) != NULL ||
        memchr(authority, ']', authority_length) != NULL) {
        fprintf(stderr, "PMSP: invalid redirect authority (IPv6 literals are not supported yet).\n");
        return -1;
    }

    colon = memchr(authority, ':', authority_length);
    if (colon) {
        if (copy_url_part(next_host, host_capacity, authority,
                          (size_t)(colon - authority)) != 0 ||
            copy_url_part(next_port, port_capacity, colon + 1,
                          authority_length - (size_t)(colon + 1 - authority)) != 0)
            return -1;
    } else {
        if (copy_url_part(next_host, host_capacity, authority, authority_length) != 0)
            return -1;
        strcpy(next_port, "443");
    }

    path_start = authority_end;
    if (*path_start == '\0' || *path_start == '#') {
        strcpy(next_path, "/");
        return 0;
    }
    if (*path_start == '?') {
        size_t query_length;
        fragment = strchr(path_start, '#');
        query_length = fragment ? (size_t)(fragment - path_start) : strlen(path_start);
        if (query_length + 1 >= path_capacity) return -1;
        next_path[0] = '/';
        memcpy(next_path + 1, path_start, query_length);
        next_path[query_length + 1] = '\0';
        return 0;
    }
    fragment = strchr(path_start, '#');
    path_length = fragment ? (size_t)(fragment - path_start) : strlen(path_start);
    return copy_url_part(next_path, path_capacity, path_start, path_length);
}

int pmsp_https_get(const char *host, const char *port, const char *path,
                   const char *ca_file, pmsp_write_callback callback,
                   void *context)
{
    char current_host[PMSP_REDIRECT_HOST_SIZE];
    char current_port[PMSP_REDIRECT_PORT_SIZE];
    char current_path[PMSP_REDIRECT_PATH_SIZE];
    char location[PMSP_REDIRECT_LOCATION_SIZE];
    char next_host[PMSP_REDIRECT_HOST_SIZE];
    char next_port[PMSP_REDIRECT_PORT_SIZE];
    char next_path[PMSP_REDIRECT_PATH_SIZE];
    unsigned int redirects = 0;
    int result;

    if (!host || !host[0] || !path || path[0] != '/' ||
        has_line_break(host) || has_line_break(path) ||
        (port && has_line_break(port)) ||
        strlen(host) >= sizeof(current_host) ||
        strlen(port ? port : PMSP_DEFAULT_PORT) >= sizeof(current_port) ||
        strlen(path) >= sizeof(current_path))
        return PMSP_ERR_ARGUMENT;

    strcpy(current_host, host);
    strcpy(current_port, port ? port : PMSP_DEFAULT_PORT);
    strcpy(current_path, path);

    for (;;) {
        location[0] = '\0';
        result = pmsp_https_get_once(current_host, current_port, current_path,
                                     ca_file, callback, context,
                                     location, sizeof(location));
        if (result != 1) return result;

        if (redirects >= PMSP_MAX_REDIRECTS) {
            fprintf(stderr, "PMSP: redirect limit (%u) exceeded.\n",
                    PMSP_MAX_REDIRECTS);
            return PMSP_ERR_HTTP;
        }

        if (resolve_redirect(current_host, current_port, current_path, location,
                             next_host, sizeof(next_host),
                             next_port, sizeof(next_port),
                             next_path, sizeof(next_path)) != 0) {
            fprintf(stderr, "PMSP: could not resolve redirect Location.\n");
            return PMSP_ERR_HTTP;
        }

        ++redirects;
        fprintf(stderr, "PMSP: following redirect %u/%u to https://%s:%s%s\n",
                redirects, PMSP_MAX_REDIRECTS,
                next_host, next_port, next_path);
        strcpy(current_host, next_host);
        strcpy(current_port, next_port);
        strcpy(current_path, next_path);
    }
}
