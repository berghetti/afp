
#include <errno.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_mbuf_core.h>
#include <rte_udp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>

#include "dpdk.h"
#include "debug.h"

#define BURST_SIZE 32

struct config
{
  uint16_t port_id;
  uint16_t num_queues;
};

struct config conf = {
  .port_id = 0,
  .num_queues = 2,
};

// simule application code...
void
app ( uint8_t *buff, size_t len )
{
  DEBUG ( "App receveid pkt with lenght %u", len );
}

static int
worker ( void *arg )
{
  uint16_t queue = ( uint16_t ) ( uintptr_t ) arg;

  INFO ( "Core %u receiving packets on queue %d.", rte_lcore_id (), queue );

  struct rte_mbuf *pkts[BURST_SIZE];
  uint16_t nb_rx;
  while ( 1 )
    {
      nb_rx = rte_eth_rx_burst ( 0, queue, pkts, BURST_SIZE );

      if ( nb_rx )
        DEBUG ( "Received %u packets on queue %d", nb_rx, queue );

      for ( int i = 0; i < nb_rx; i++ )
        {
          struct rte_mbuf *pkt = pkts[i];

          struct rte_ether_hdr *eth_hdr =
                  rte_pktmbuf_mtod ( pkt, struct rte_ether_hdr * );

          struct rte_ether_addr tmp = eth_hdr->dst_addr;
          eth_hdr->dst_addr = eth_hdr->src_addr;
          eth_hdr->src_addr = tmp;

          uint32_t offset = sizeof ( struct rte_ether_hdr );
          struct rte_ipv4_hdr *ip_hdr =
                  rte_pktmbuf_mtod_offset ( pkt,
                                            struct rte_ipv4_hdr *,
                                            offset );

          // assume no options header IP
          offset += sizeof ( struct rte_ipv4_hdr );
          struct rte_udp_hdr *udp_hdr =
                  rte_pktmbuf_mtod_offset ( pkt, struct rte_udp_hdr *, offset );

          offset += sizeof ( struct rte_udp_hdr );
          uint8_t *payload = rte_pktmbuf_mtod_offset ( pkt, uint8_t *, offset );

          size_t size_payload = rte_pktmbuf_pkt_len ( pkt ) - offset;
          app ( payload, size_payload );
        }

      rte_eth_tx_burst ( 0, queue, pkts, nb_rx );
    }

  return 0;
}

static void
dataplane ( void )
{
  uint16_t port = 0;
  if ( rte_eth_dev_socket_id ( port ) >= 0 &&
       rte_eth_dev_socket_id ( port ) != ( int ) rte_socket_id () )
    INFO ( "WARNING, port %u is on remote NUMA node to "
           "polling thread.\n\tPerformance will "
           "not be optimal.\n",
           port );

  unsigned int core_id;
  uint16_t queue_index = 0;
  RTE_LCORE_FOREACH_WORKER ( core_id )
  {
    if ( rte_eal_remote_launch ( worker,
                                 ( void * ) ( uintptr_t ) queue_index++,
                                 core_id ) )
      ERROR ( "Error start worker on core id %d: %s",
              core_id,
              rte_strerror ( rte_errno ) );
  }
}

int
main ( int argc, char **argv )
{
  int ret;

  ret = rte_eal_init ( argc, argv );
  if ( ret < 0 )
    rte_panic ( "Cannot init EAL\n" );

  if ( rte_lcore_count () < 3 )
    ERROR ( "%s", "Minimmum of cores should be 3" );

  INFO ( "Cores available %d. Main core %d.",
         rte_lcore_count (),
         rte_get_main_lcore () );

  dpdk_init ( rte_lcore_count () - 1 );
  dataplane ();

  /* clean up the EAL */
  rte_eal_cleanup ();

  return 0;
}
