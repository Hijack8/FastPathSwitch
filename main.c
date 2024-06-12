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

struct Switch
{
	int n_lcores;
	int lcores[APP_MAX_LCORES];
	int promiscuous_on;
	struct rte_mempool *pool;

	int port_mask;
	int n_queues;
	int n_ports;
	int ports[APP_MAX_PORTS];
} app;

static struct rte_eth_conf port_conf = {
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

static int
app_init_port()
{
}

void app_init()
{
	// init mempool
	app.pool = rte_mempool_create_empty("switch pool", (1 << 15) - 1, 64, 0, 0, rte_socket_id(), 0);
	if (app.pool == NULL)
	{
		rte_panic("alloc mempool failed");
	}

	app_init_port();
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

static int app_launch_one_lcore(__rte_unused void *dummy)
{
	app_main_loop();
	return 0;
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
	RTE_LCORE_FOREACH_WORKER(lcore_id)
	{
		/* Simpler equivalent. 8< */
		rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
		/* >8 End of simpler equivalent. */
	}

	/* call it on main lcore too */
	lcore_hello(NULL);
	/* >8 End of launching the function on each lcore. */

	rte_eal_mp_wait_lcore();

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
