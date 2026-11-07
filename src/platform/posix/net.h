#ifndef PMSP_POSIX_NET_H
#define PMSP_POSIX_NET_H
#include <stddef.h>
int pmsp_net_connect(const char *host, const char *port);
void pmsp_net_close(int socket_fd);
int pmsp_net_send(void *context, const unsigned char *buffer, size_t length);
int pmsp_net_recv(void *context, unsigned char *buffer, size_t length);
#endif
