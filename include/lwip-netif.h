
#ifndef __LWIP_NETIF_H__
#define __LWIP_NETIF_H__

#include <stdio.h>
#include <string.h>

#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/ip_addr.h>
#include <lwip/etharp.h>
#include <lwip/dhcp.h>

#include <uk/print.h>
#include <uk/netdev.h>
#include <uk/thread.h>
#include <uk/semaphore.h>

void start_networking(void);

#endif /*__LWIP_NETIF_H__ */