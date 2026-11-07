#define _POSIX_C_SOURCE 200112L

#include "platform/posix/net.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <mbedtls/ssl.h>

int pmsp_net_connect(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    int socket_fd = -1;

    if (host == NULL || port == NULL) {
        return -1;
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    if (getaddrinfo(host, port, &hints, &addresses) != 0) {
        return -1;
    }

    for (address = addresses; address != NULL; address = address->ai_next) {
        socket_fd = socket(address->ai_family,
                           address->ai_socktype,
                           address->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        if (connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(addresses);
    return socket_fd;
}

void pmsp_net_close(int socket_fd)
{
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

int pmsp_net_send(void *context, const unsigned char *buffer, size_t length)
{
    int fd = *(int *)context;
    ssize_t n;

    do {
        n = send(fd, buffer, length, 0);
    } while (n < 0 && errno == EINTR);

    if (n >= 0) {
        return (int)n;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }

    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

int pmsp_net_recv(void *context, unsigned char *buffer, size_t length)
{
    int fd = *(int *)context;
    ssize_t n;

    do {
        n = recv(fd, buffer, length, 0);
    } while (n < 0 && errno == EINTR);

    if (n >= 0) {
        return (int)n;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }

    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
