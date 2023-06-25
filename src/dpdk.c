
#include <rte_errno.h>
#include <rte_lcore.h>
#include <stdint.h>
#include <stdlib.h>

#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <sys/types.h>

#include "debug.h"

static const struct rte_eth_conf conf = {
  .rxmode = { .mq_mode = RTE_ETH_MQ_RX_RSS,
              .offloads = RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
                          RTE_ETH_RX_OFFLOAD_UDP_CKSUM },
  .rx_adv_conf = { .rss_conf = { .rss_hf = RTE_ETH_RSS_IPV4 |
                                           /*RTE_ETH_RSS_NONFRAG_IPV4_TCP | */
                                           RTE_ETH_RSS_NONFRAG_IPV4_UDP } },
  .txmode = { .offloads = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
                          RTE_ETH_TX_OFFLOAD_UDP_CKSUM,
              .mq_mode = RTE_ETH_MQ_TX_NONE },
};

#define QUEUE_SIZE 4096U

#define MPOOL_NR_ELEMENTS 32 * 1024 - 1
#define MPOOL_CACHE_SIZE 512U

static void
port_init ( uint16_t port_id, int num_queues, struct rte_mempool *mbuf_pool )
{
  uint16_t rx_queues = num_queues;
  uint16_t tx_queues = num_queues;

  int ret;
  ret = rte_eth_dev_configure ( port_id, rx_queues, tx_queues, &conf );
  if ( ret != 0 )
    ERROR ( "%s", "Error config port id 0\n" );

  uint16_t nb_rx = QUEUE_SIZE;
  uint16_t nb_tx = QUEUE_SIZE;

  ret = rte_eth_dev_adjust_nb_rx_tx_desc ( port_id, &nb_rx, &nb_tx );
  if ( ret != 0 )
    rte_exit ( EXIT_FAILURE, "Error set queue size" );

  int numa_id = rte_eth_dev_socket_id ( port_id );

  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get ( 0, &dev_info );

  /* Allocate and set up QUEUE_COUNT RX/TX queue per Ethernet port. */

  struct rte_eth_rxconf *rxconf = &dev_info.default_rxconf;
  rxconf->offloads =
          RTE_ETH_RX_OFFLOAD_IPV4_CKSUM | RTE_ETH_RX_OFFLOAD_UDP_CKSUM;
  uint16_t q;
  for ( q = 0; q < rx_queues; q++ )
    {
      ret = rte_eth_rx_queue_setup ( port_id,
                                     q,
                                     nb_rx,
                                     numa_id,
                                     rxconf,
                                     mbuf_pool );
      if ( ret != 0 )
        rte_exit ( EXIT_FAILURE, "Error allocate RX queue" );
    }

  struct rte_eth_txconf *txconf = &dev_info.default_txconf;
  txconf->tx_rs_thresh = 64;
  txconf->tx_free_thresh = 64;
  txconf->offloads =
          RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | RTE_ETH_TX_OFFLOAD_UDP_CKSUM;

  for ( q = 0; q < tx_queues; q++ )
    {
      ret = rte_eth_tx_queue_setup ( port_id, q, nb_tx, numa_id, txconf );
      if ( ret != 0 )
        rte_exit ( EXIT_FAILURE, "Error allocate TX queue\n" );
    }

  ret = rte_eth_dev_start ( port_id );
  if ( ret != 0 )
    rte_exit ( EXIT_FAILURE, "Error start port id 0\n" );

  /* Display the port MAC address. */
  struct rte_ether_addr addr;
  ret = rte_eth_macaddr_get ( port_id, &addr );
  if ( ret != 0 )
    rte_exit ( EXIT_FAILURE, "Error get mac address" );

  INFO ( "Using port with MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
         ":%02" PRIx8 ":%02" PRIx8 "\n",
         RTE_ETHER_ADDR_BYTES ( &addr ) );
}

struct rte_mempool *
dpdk_init ( uint16_t port_id, uint16_t num_queues )
{
  struct rte_mempool *mbuf_pool;

  mbuf_pool = rte_pktmbuf_pool_create ( "mbuf_pool",
                                        MPOOL_NR_ELEMENTS,
                                        MPOOL_CACHE_SIZE,
                                        0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id () );

  if ( !mbuf_pool )
    ERROR ( "Error alloc mbuf on socket %d: %s\n",
            rte_socket_id (),
            rte_strerror ( rte_errno ) );

  port_init ( port_id, num_queues, mbuf_pool );

  return mbuf_pool;
}
