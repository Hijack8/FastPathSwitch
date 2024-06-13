/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <getopt.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_prefetch.h>
#include <rte_ethdev.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>

#define APP_MAX_LCORES 16
#define APP_MAX_PORTS 16

// configurable number of RX/TX ring descriptors
#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RX_DESC_DEFAULT;
static uint16_t nb_txd = TX_DESC_DEFAULT;

struct lcore_queue_conf
{
	unsigned n_ports;
	unsigned rx_port_list[APP_MAX_PORTS];
};
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

#define MAC_TABLE_SIZE 20

struct Switch
{
	int n_lcores;
	int lcores[APP_MAX_LCORES];
	struct rte_mempool *pool;
	int port_mask;
	int n_ports;
	int ports[APP_MAX_PORTS];

	int mac_table[MAC_TABLE_SIZE];
	struct rte_hash *hash;
} app;

static struct rte_eth_conf port_conf = {
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

int init_lcore_rx_queues()
{
	uint16_t i, n_rx_queues;
	uint8_t lcore;
	for (i = 0; i < APP_MAX_LCORES; i++)
	{
	}
}

static int app_init_lcores()
{
	int n_lcore = 0;
	for (int i = 0; i < APP_MAX_LCORES; i++)
	{
		if (!rte_lcore_is_enabled(i))
			continue;
		app.lcores[n_lcore++] = i;
	}
	app.n_lcores = n_lcore;
}

static int
app_init_port()
{
	int port_id;
	int ret;
	for (port_id = 0; port_id < APP_MAX_PORTS; port_id++)
	{
		if ((app.port_mask & (1 << port_id)) == 0)
			continue;
		app.ports[app.n_ports++] = port_id;
	}

	printf("n_ports = %d \n", app.n_ports);
	printf("n_lcores = %d \n", app.n_lcores);

	if (app.n_ports > app.n_lcores)
		rte_exit(EXIT_FAILURE, "n_ports > n_lcores \n");

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues failed\n");
	int n_tx_queue = app.n_ports;
	struct rte_eth_conf local_port_conf = port_conf;
	struct rte_eth_dev_info dev_info;
	for (int i = 0; i < app.n_ports; i++)
	{
		struct rte_eth_rxconf rxq_conf;
		struct rte_eth_txconf txq_conf;
		port_id = app.ports[i];
		ret = rte_eth_dev_configure(port_id, 1, n_tx_queue, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device err = %d port = %d \n", ret, port_id);
		ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot adjust number of descriptors err = %d port = %d \n", ret, port_id);

		ret = rte_eth_dev_info_get(port_id, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE, "Error during getting device info\n");

		rxq_conf = dev_info.default_rxconf;
		txq_conf = dev_info.default_txconf;
		ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd, rte_socket_id(), &rxq_conf, app.pool);
		if (ret != 0)
			rte_exit(EXIT_FAILURE, "Error rx queue setup \n");

		for (int queue_id = 0; queue_id < n_tx_queue; queue_id++)
		{
			ret = rte_eth_tx_queue_setup(port_id, queue_id, nb_txd, rte_socket_id(), &txq_conf);
			if (ret != 0)
				rte_exit(EXIT_FAILURE, "Error tx queue setup \n");
		}
	}

	for (int i = 0; i < app.n_ports; i++)
	{
		port_id = app.ports[i];
		ret = rte_eth_dev_start(port_id);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start \n");
        rte_eth_promiscuous_enable(port_id);
	}
}

void app_init_hash()
{
	char name[10] = "hash_name";
	struct rte_hash_parameters hash_params = {
		.name = name,
		.entries = MAC_TABLE_SIZE,
		.key_len = sizeof(struct rte_ether_addr),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
	};
	app.hash = rte_hash_create(&hash_params);
}

void app_init()
{
	// init mempool
	// TODO
	app.pool = rte_pktmbuf_pool_create("switch_pool", (1 << 15) - 1, 256, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (app.pool == NULL)
	{
		rte_panic("alloc mempool failed");
	}

	app_init_lcores();
	app_init_port();
	app_init_hash();
}

/* Launch a function on lcore. 8< */
static int
lcore_hello(__rte_unused void *arg)
{
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	printf("hello from core %u\n", lcore_id);
	return 0;
}
/* >8 End of launching function on lcore. */

static const char short_options[] =
	"p:" // port mask
	;

int app_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || end == NULL || *end != '\0')
		return 0;
	return pm;
}

int app_parse_args(int argc, char **argv)
{
	int opt, option_index, ret;
	int p, q;
	char **argvopt;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{"none", 0, 0, 0},
	};
	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, short_options, lgopts, &option_index)) != EOF)
	{
		switch (opt)
		{
		case 'p':
			app.port_mask = app_parse_portmask(optarg);
			if (app.port_mask == 0)
			{
				printf("invalid port mask \n");
				return -1;
			}
			printf("port_mask = %x\n", app.port_mask);
			break;
		default:
			printf("invalid\n");
			return -1;
		}
	}
	if (optind >= 0)
		argv[optind - 1] = prgname;
	ret = optind - 1;
	optind = 1;
	return ret;
}

void print_addr(struct rte_ether_addr *addr) {
    printf("MAC Address : %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 "\n",
           addr->addr_bytes[0], addr->addr_bytes[1], addr->addr_bytes[2],
           addr->addr_bytes[3], addr->addr_bytes[4], addr->addr_bytes[5]);
}

int app_l2_lookup(const struct rte_ether_addr *addr)
{
	int index = rte_hash_lookup(app.hash, addr);
	if (index >= 0 && index < MAC_TABLE_SIZE)
	{
		return app.mac_table[index];
	}
	return -1;
}


void l2_learning(struct rte_mbuf *m, int src_port)
{
	struct rte_ether_hdr *eth;
	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	struct rte_ether_addr *addr;
	addr = &eth->src_addr;
	if (app_l2_lookup(addr) >= 0)
		return;
	int index = rte_hash_add_key(app.hash, addr);
    printf("learning ... port_id = %u\n", src_port);
	if (index < 0)
		rte_panic("l2_hash add key error \n");
    printf("index = %d \n", index);
	app.mac_table[index] = src_port;
}

int get_dest_port(struct rte_mbuf *m, int src_port)
{
	struct rte_ether_hdr *eth;
	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	struct rte_ether_addr *dst_addr;
	dst_addr = &eth->dst_addr;
	return app_l2_lookup(dst_addr);
}

#define BURST_SIZE 32

int app_main_loop(void *)
{
	uint32_t lcore_id = rte_lcore_id();
	printf("lcore %u is forwarding \n", lcore_id);

	int port = app.ports[lcore_id];

	struct rte_mbuf *bufs[BURST_SIZE];
	struct rte_mbuf *tx_bufs[APP_MAX_PORTS][BURST_SIZE];
	int n_tx_bufs[APP_MAX_PORTS] = {0};
	while (1)
	{
		int nb_rx = rte_eth_rx_burst(port, 0, bufs, 8);

		if (nb_rx == 0)
			continue;

		for (int i = 0; i < nb_rx; i++)
		{
			l2_learning(bufs[i], port);
			int dst_port = get_dest_port(bufs[i], port);
            if(dst_port == -1) {
                // broadcast 
                for(int j = 0; j < app.n_ports; j ++) {
                    if(j != (port ^ 1) && j != port)
                    {
			            tx_bufs[j][n_tx_bufs[j]++] = rte_pktmbuf_clone(bufs[i], app.pool);
                    }
                }
			    tx_bufs[port ^ 1][n_tx_bufs[port ^ 1]++] = bufs[i];
            }
            else 
			    tx_bufs[dst_port][n_tx_bufs[dst_port]++] = bufs[i];
		}
		for (int i = 0; i < app.n_ports; i++)
		{
			if (n_tx_bufs[i] == 0)
				continue;
			int nb_tx = rte_eth_tx_burst(i, lcore_id, tx_bufs[i], n_tx_bufs[i]);
			if (unlikely(nb_tx < n_tx_bufs[i]))
			{
				for (int j = nb_tx; j < n_tx_bufs[i]; j++)
					rte_pktmbuf_free(tx_bufs[i][j]);
			}
            n_tx_bufs[i] = 0;
		}
	}
	return 0;
}

void app_finish()
{
	int ret;
	for (int i = 0; i < app.n_ports; i++)
	{
		int port_id = app.ports[i];
		ret = rte_eth_dev_stop(port_id);
		if (ret != 0)
			printf("rte_eth_dev_stop err \n");
	}
}

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */

	argc -= ret;
	argv += ret;

	// parse app arguments
	app_parse_args(argc, argv);

	// init
	app_init();

	/* Launches the function on each lcore. 8< */
	rte_eal_mp_remote_launch(app_main_loop, NULL, CALL_MAIN);

	app_finish();

	rte_eal_mp_wait_lcore();

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
