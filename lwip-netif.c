
#include <lwip-netif.h>

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

#define MAX_INTERFACES 2

/* Network interfaces and netdevs are associated 1:1 */
static struct uk_netdev *netdev[MAX_INTERFACES];
static struct netif *netif_array[MAX_INTERFACES];

/**
 * Is called from the lwip thread when finishing set up.
 */
struct uk_semaphore tcpip_is_up[MAX_INTERFACES];
static void tcpip_bringup_finished(void *p)
{
	struct netif *netif = (struct netif *)p;
	uk_printd(DLVL_INFO, "TCP/IP bring up ends. Netif%d\n", netif->num);
	uk_semaphore_up(&tcpip_is_up[netif->num]);
}

/**
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 */
static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
	struct uk_netdev *dev = netdev[netif->num];
	if (!dev)
		return ERR_OK;
#ifdef ETH_PAD_SIZE
	pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

	/* Send the data from the pbuf to the interface, one pbuf at a
	   time. The size of the data in each pbuf is kept in the ->len
	   variable. */
	if (!p->next) {
		/* Only one fragment, can send it directly */
		uk_netdev_tx(dev, p->payload, p->len);
	} else {
		unsigned char data[p->tot_len], *cur;
		struct pbuf *q;

		for(q = p, cur = data; q != NULL; cur += q->len, q = q->next)
			memcpy(cur, q->payload, q->len);
		uk_netdev_tx(dev, data, p->tot_len);
	}

#if ETH_PAD_SIZE
	pbuf_header(p, ETH_PAD_SIZE);			/* reclaim the padding word */
#endif

	LINK_STATS_INC(link.xmit);

	return ERR_OK;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface.
 * Pulls received packets into a pbuf queue for the low_level_input()
 * function to pass up to lwIP.
 */
static void
netfront_input(struct netif *netif, unsigned char* data, int len)
{
	struct pbuf *p = NULL;

	if (len >= 0) {
		p = pbuf_alloc(PBUF_RAW, (u16_t) len, PBUF_POOL);
		if (p != NULL) {
#if ETH_PAD_SIZE
			pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif
			pbuf_take(p, data, (u16_t) len);

			/* acknowledge that packet has been read(); */
		} else {
			/* drop packet(); */
			LWIP_DEBUGF(NETIF_DEBUG, ("could not allocate pbuf\n"));
		}
	}
	if ((p != NULL) && (netif->input(p, netif) != ERR_OK)) {
		LWIP_DEBUGF(NETIF_DEBUG, ("netif input error\n"));
		pbuf_free(p);
	}
#if ETH_PAD_SIZE
	else {
		pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
	}
#endif
}

/**
 * Used in the driver polling mode.
 * Must be run on a dedicated thread.
 *
 * @param arg
 *     The network interface on which to send packets up to the stack
 */
void poll_driver(void* arg) {
	int len;
	void *data;
	struct netif *netif = (struct netif *)arg;
	struct uk_netdev *dev = netdev[netif->num];

	data = malloc(dev->data->mtu);

	while (1) {
		uk_sched_thread_sleep(10);
		len = uk_netdev_rx(dev, data, dev->data->mtu);
		netfront_input(netif, data, len);
	}
}

/**
 * Used by the driver interrupt callback.
 */
void netif_rx(uint16_t id, uint16_t queue, void *data, uint16_t len)
{
	if (netif_array[id] != NULL && (netif_array[id]->flags & NETIF_FLAG_UP)) {
		netfront_input(netif_array[id], data, len);
	}
	/* By returning, we ack the packet and relinquish the RX ring slot */
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface.
 * Sent as parameter and called by netif_add().
 */
err_t
netif_netfront_init(struct netif *netif)
{
	unsigned char *mac = netif->state;

	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;

	/* set MAC hardware address */
	netif->hwaddr_len = 6;
	netif->hwaddr[0] = mac[0];
	netif->hwaddr[1] = mac[1];
	netif->hwaddr[2] = mac[2];
	netif->hwaddr[3] = mac[3];
	netif->hwaddr[4] = mac[4];
	netif->hwaddr[5] = mac[5];

	/* No interesting per-interface state */
	netif->state = NULL;

	/* maximum transfer unit */
	netif->mtu = netdev[netif->num]->data->mtu;

	/* broadcast capability */
	netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
					NETIF_FLAG_LINK_UP | NETIF_FLAG_ETHERNET;

	return ERR_OK;
}

/**
 * Initialization function, configures one uk_netdev and associates it with
 * an lwip network interface.
 * Configures a static IP address if provided by the driver or starts a DHCP
 * client otherwise.
 * */
void setup_netif(uint8_t num)
{
	struct uk_netdev_conf conf = { .requested_mode = UK_NETDEV_MODE_INTERRUPT };

	netdev[num] = uk_netdev_get(num);
	if (uk_netdev_configure(netdev[num], &conf) < 0) {
		uk_printd(DLVL_ERR, "Can not configure netdev%d\n", num);
		return;
	}

	struct uk_netdev_rxconf rxconf = { .rx_cb = netif_rx };
	uk_netdev_rx_queue_setup(netdev[num], 0, &rxconf);
	uk_netdev_tx_queue_setup(netdev[num], 0, NULL);
	uk_netdev_start(netdev[num]);
}

static void netif_status(struct netif *netif)
{
	struct uk_netdev *n = NULL;
	int i;

	for (i = 0; i < uk_netdev_count(); i++) {
		if (netif_array[i] == netif) {
			n = netdev[i];
			break;
		}
	}

	UK_ASSERT(n != NULL);

	uk_printd(DLVL_INFO, "netif: IP address of interface %s set to %u.%u.%u.%u\n",
		n->data->name,
	    ip4_addr1_16(netif_ip4_addr(netif)),
	    ip4_addr2_16(netif_ip4_addr(netif)),
	    ip4_addr3_16(netif_ip4_addr(netif)),
	    ip4_addr4_16(netif_ip4_addr(netif)));
}

void setup_netif2(uint8_t num)
{
	ip4_addr_t *ipaddr = mem_malloc(sizeof(ip4_addr_t));
	ip4_addr_t *netmaskaddr = mem_malloc(sizeof(ip4_addr_t));
	ip4_addr_t *gwaddr = mem_malloc(sizeof(ip4_addr_t));

	netif_array[num] = malloc(sizeof(struct netif));
	memset(netif_array[num], 0, sizeof(struct netif));

	uk_printd(DLVL_INFO, "TCP/IP bring up begins.\n");

	uk_semaphore_init(&tcpip_is_up[num], 0);
	tcpip_init(tcpip_bringup_finished, netif_array[num]);

	uk_ip_info_t *ip_info = uk_netdev_ip_get(netdev[num]);
	if (ip_info == NULL) {
		uk_printd(DLVL_INFO, "NULL ip_info\n");
		ipaddr->addr = 0;
		netmaskaddr->addr = 0;
		gwaddr->addr = 0;
	} else {
		uk_printd(DLVL_INFO, "Got some ip_info %d\n", ip_info->ip.ipv4_addr);
		ipaddr->addr = ip_info->ip.ipv4_addr;
		netmaskaddr->addr = ip_info->netmask.ipv4_addr;
		gwaddr->addr = ip_info->gateway.ipv4_addr;
	}

	netif_array[num]->num = num;
	if (netif_add(netif_array[num], ipaddr, netmaskaddr, gwaddr,
			  netdev[num]->data->mac_addr.addr_bytes,
			  netif_netfront_init, tcpip_input) == NULL) {
		uk_printd(DLVL_ERR, "Error adding netif\n");
	}
	netif_set_up(netif_array[num]);

 	uk_semaphore_down(&tcpip_is_up[num]);

	if (ip_info == NULL) {
		uk_printd(DLVL_INFO, "No IP from driver, starting DHCP\n");

		netif_array[num]->status_callback = netif_status;

		int err = dhcp_start(netif_array[num]);
		if (err != ERR_OK) {
			uk_printd(DLVL_ERR, "Error starting DHCP client. ERR %d\n", err);
		}

		if (dhcp_supplied_address(netif_array[num]) == 0) {
			uk_printd(DLVL_ERR, "Failed to get DHCP address\n");
		}
	}
}

/**
 * Initialization utility function to bring the whole lot up.
 */
void start_networking(void)
{
	uint8_t i;

	uk_printd(DLVL_INFO, "Setting up network interfaces.\n");

	for (i = 0; i < uk_netdev_count(); i++) {
		setup_netif(i);
	}
}

void start_networking2(void)
{
	uint8_t i;

	uk_printd(DLVL_INFO, "Waiting for network.\n");

	for (i = 0; i < uk_netdev_count(); i++) {
		setup_netif2(i);
	}
	netif_set_default(netif_array[0]);

	for (i = 0; i < uk_netdev_count(); i++) {
		uk_printd(DLVL_INFO, "NETIF%d mac: %hhx:%hhx:%hhx:%hhx:%hhx:%hhx\n", i,
				  netdev[i]->data->mac_addr.addr_bytes[0],
				  netdev[i]->data->mac_addr.addr_bytes[1],
				  netdev[i]->data->mac_addr.addr_bytes[2],
				  netdev[i]->data->mac_addr.addr_bytes[3],
				  netdev[i]->data->mac_addr.addr_bytes[4],
				  netdev[i]->data->mac_addr.addr_bytes[5]);
		if (netdev[i]->data->driver_mode == UK_NETDEV_MODE_POLLING) {
			uk_printd(DLVL_INFO, "Starting driver poll\n");
			uk_thread_create("poll_driver", poll_driver, netif_array[i]);
		}
	}

	uk_printd(DLVL_INFO, "Network is ready.\n");
}

