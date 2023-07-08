/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_byteorder.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <stdint.h>

#include "dpdk.h"

extern void
stats_init ( void );

extern void
flow_insert ( uint16_t port_id, uint16_t rxq, uint16_t udp_src_port );

#define PORT_ID 0

#define SWAP( a, b )           \
  ( {                          \
    typeof ( a ) _tmp = ( a ); \
    ( a ) = ( b );             \
    ( b ) = _tmp;              \
  } )

static struct rte_ether_addr my_ether;

static __thread uint16_t hwq;

static void
send_pkt ( struct rte_mbuf *pkt )
{
  // offloading cksum to hardware
  // pkt->ol_flags |= ( RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM |
  //                   RTE_MBUF_F_TX_UDP_CKSUM );

  struct rte_ether_hdr *eth_hdr;
  eth_hdr = rte_pktmbuf_mtod ( pkt, struct rte_ether_hdr * );
  eth_hdr->dst_addr = eth_hdr->src_addr;
  eth_hdr->src_addr = my_ether;

  uint32_t offset = sizeof ( struct rte_ether_hdr );
  struct rte_ipv4_hdr *ip_hdr;
  ip_hdr = rte_pktmbuf_mtod_offset ( pkt, struct rte_ipv4_hdr *, offset );
  // ip_hdr->ihl = 5;
  // ip_hdr->version = 4;
  // ip_hdr->total_length =
  //        rte_cpu_to_be_16 ( sizeof ( struct rte_ipv4_hdr ) +
  //                           sizeof ( struct rte_udp_hdr ) + len );

  // no IP fragments
  // ip_hdr->packet_id = 0;
  // ip_hdr->fragment_offset = 0;

  // ip_hdr->time_to_live = 64;
  // ip_hdr->next_proto_id = IPPROTO_UDP;
  // ip_hdr->hdr_checksum = 0;
  SWAP ( ip_hdr->src_addr, ip_hdr->dst_addr );

  offset += sizeof ( struct rte_ipv4_hdr );
  struct rte_udp_hdr *udp_hdr;
  udp_hdr = rte_pktmbuf_mtod_offset ( pkt, struct rte_udp_hdr *, offset );

  SWAP ( udp_hdr->src_port, udp_hdr->dst_port );

  // udp_hdr->dgram_len = rte_cpu_to_be_16 ( sizeof ( struct rte_udp_hdr ) + len
  // ); udp_hdr->dgram_cksum = 0;

  // offset += sizeof ( struct rte_udp_hdr );
  // void *payload = rte_pktmbuf_mtod_offset ( pkt, void *, offset );
  // rte_memcpy ( payload, buff, len );

  // TODO: need this?
  // pkt->l2_len = RTE_ETHER_HDR_LEN;
  // pkt->l3_len = sizeof ( struct rte_ipv4_hdr );
  // pkt->l4_len = sizeof ( struct rte_udp_hdr ) + len;

  //// Total size packet
  // pkt->pkt_len = pkt->data_len = MIN_PKT_SIZE + len;

  rte_eth_tx_burst ( PORT_ID, hwq, &pkt, 1 );
}

#define BURST_SIZE 8
#define DELAY 100

/* Launch a function on lcore. 8< */
static int
lcore_hello ( void *arg )
{
  hwq = ( uint16_t ) ( uintptr_t ) arg;

  printf ( "started worker %u\n", hwq );

  uint16_t nb_rx;
  struct rte_mbuf *pkts[BURST_SIZE];
  while ( 1 )
    {
      nb_rx = rte_eth_rx_burst ( PORT_ID, hwq, pkts, BURST_SIZE );

      for ( uint16_t i = 0; i < nb_rx; i++ )
        {
          // struct rte_udp_hdr *udp_hdr;
          // udp_hdr = rte_pktmbuf_mtod_offset (
          //        pkts[i],
          //        struct rte_udp_hdr *,
          //        sizeof ( struct rte_ether_hdr ) +
          //                sizeof ( struct rte_ipv4_hdr ) );

          // printf ( "queue %u received port %u\n", hwq, udp_hdr->src_port );

          rte_delay_us_block ( DELAY );

          send_pkt ( pkts[i] );
        }
    }
}
/* >8 End of launching function on lcore. */

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main ( int argc, char **argv )
{
  int ret;
  unsigned lcore_id;

  ret = rte_eal_init ( argc, argv );
  if ( ret < 0 )
    rte_panic ( "Cannot init EAL\n" );
  /* >8 End of initialization of Environment Abstraction Layer */

  int tot_queues = rte_lcore_count ();

  dpdk_init ( PORT_ID, tot_queues );

  uint16_t ports_src[] = { 1024, 1025, 1026, 1027, 1028, 1029,
                           1030, 1031, 1032, 1033, 1034, 1035 };

  for ( int i = 0; i < sizeof ( ports_src ) / sizeof ( ports_src[0] ); i++ )
    {
      uint16_t queue = i % tot_queues;
      printf ( "Setting port %u to queue %u\n", ports_src[i], queue );

      flow_insert ( PORT_ID, queue, ports_src[i] );
    }

  stats_init ();

  rte_eth_macaddr_get ( PORT_ID, &my_ether );

  int i = 0;
  /* Launches the function on each lcore. 8< */
  RTE_LCORE_FOREACH_WORKER ( lcore_id )
  {
    /* Simpler equivalent. 8< */
    rte_eal_remote_launch ( lcore_hello,
                            ( void * ) ( uintptr_t ) i++,
                            lcore_id );
    /* >8 End of simpler equivalent. */
  }

  /* call it on main lcore too */
  lcore_hello ( ( void * ) ( uintptr_t ) i );
  /* >8 End of launching the function on each lcore. */

  rte_eal_mp_wait_lcore ();

  /* clean up the EAL */
  rte_eal_cleanup ();

  return 0;
}
