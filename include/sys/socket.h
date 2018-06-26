#ifndef UK_LWIP_SOCKET_H
#define UK_LWIP_SOCKET_H
#include <uk/config.h>

#define AF_UNIX 1

#if CONFIG_LIBLWIP
#include <lwip/inet.h>
#include <lwip/sockets.h>
#endif /* CONFIG_LIBLWIP */

#define SOCK_CLOEXEC    0x10000000
#define SOCK_NONBLOCK   0x20000000

#define TCP_FASTOPEN	1025
#define IPPROTO_IPV6	41

#define  MSG_TRUNC       0x10 
#define  MSG_CTRUNC      0x20

struct cmsghdr {
               size_t cmsg_len;    /* Data byte count, including header
                                      (type is socklen_t in POSIX) */
               int    cmsg_level;  /* Originating protocol */
               int    cmsg_type;   /* Protocol-specific type */
           /* followed by
              unsigned char cmsg_data[]; */
};

#endif /* UK_LWIP_SOCKET_H */
