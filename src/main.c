
#include <errno.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_mbuf_core.h>
#include <rte_memcpy.h>
#include <rte_udp.h>
#include <stddef.h>
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

static struct rte_mempool *pkt_pool;

struct config
{
  uint16_t port_id;
  uint16_t num_queues;
};

struct config conf = {
  .port_id = 0,
  .num_queues = 2,
};

struct afp_ctx
{
  uint16_t queue;
};

typedef struct afp_ctx afp_ctx_t;

#define SWAP( a, b )           \
  ( {                          \
    typeof ( a ) _tmp = ( a ); \
    ( a ) = ( b );             \
    ( b ) = _tmp;              \
  } )

struct sock
{
  struct rte_ether_hdr eth_hdr;
  struct rte_ipv4_hdr ip_hdr;
  struct rte_udp_hdr udp_hdr;
};

size_t
afp_recv ( afp_ctx_t *ctx, uint8_t *buff, size_t len )
{
  struct rte_mbuf *pkts[BURST_SIZE];
  uint16_t nb_rx;

  uint16_t queue = ctx->queue;
  // DEBUG ( "checking queue %u \n", queue );
  nb_rx = rte_eth_rx_burst ( 0, queue, pkts, BURST_SIZE );

  if ( nb_rx )
    DEBUG ( "Received %u packets on queue %d\n", nb_rx, queue );

  for ( int i = 0; i < nb_rx; i++ )
    {
      struct rte_mbuf *pkt = pkts[i];

      // sock->eth_hdr.src_addr =
      //        rte_pktmbuf_mtod ( pkt, struct rte_ether_hdr * )->src_addr;
      // sock->eth_hdr.dst_addr =
      //        rte_pktmbuf_mtod ( pkt, struct rte_ether_hdr * )->dst_addr;

      uint32_t offset =
              sizeof ( struct rte_ether_hdr ) +
              sizeof ( struct rte_ipv4_hdr ) + /* assume no options IP header */
              sizeof ( struct rte_udp_hdr );

      uint8_t *payload = rte_pktmbuf_mtod_offset ( pkt, uint8_t *, offset );
      size_t size_payload = rte_pktmbuf_pkt_len ( pkt ) - offset;

      //*buff = payload;
      //*len = size_payload;

      rte_memcpy ( buff, payload, len );
      rte_pktmbuf_free ( pkt );

      return size_payload;
    }

  return 0;
}

static struct rte_ether_addr my_ether;
static struct rte_ether_addr client_ether = {
  .addr_bytes = { 0x00, 0x11, 0x22, 0x33, 0x02, 0x01 },
};

#define MAKE_IP_ADDR( a, b, c, d )                        \
  ( ( ( uint32_t ) a << 24 ) | ( ( uint32_t ) b << 16 ) | \
    ( ( uint32_t ) c << 8 ) | ( uint32_t ) d )

ssize_t
afp_send ( afp_ctx_t *ctx, uint8_t *buff, size_t len )
{
  DEBUG ( "%s\n", "send pkt" );
  struct rte_mbuf *pkt;

  pkt = rte_pktmbuf_alloc ( pkt_pool );
  uint16_t pkt_size = sizeof ( struct rte_ether_hdr ) +
                      sizeof ( struct rte_ipv4_hdr ) +
                      sizeof ( struct rte_udp_hdr ) + len;

  if ( !rte_pktmbuf_append ( pkt, pkt_size ) )
    ERROR ( "%s", "Error allocate pkt tx\n" );

  DEBUG ( "Packet len %u\n", rte_pktmbuf_pkt_len ( pkt ) );

  // need this to offloading cksum to hardware
  pkt->ol_flags |= ( RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM |
                     RTE_MBUF_F_TX_UDP_CKSUM );

  struct rte_ether_hdr *eth_hdr;
  eth_hdr = rte_pktmbuf_mtod ( pkt, struct rte_ether_hdr * );
  eth_hdr->src_addr = my_ether;
  eth_hdr->dst_addr = client_ether;
  eth_hdr->ether_type = rte_cpu_to_be_16 ( RTE_ETHER_TYPE_IPV4 );

  uint32_t offset = sizeof ( struct rte_ether_hdr );
  struct rte_ipv4_hdr *ip_hdr;
  ip_hdr = rte_pktmbuf_mtod_offset ( pkt, struct rte_ipv4_hdr *, offset );
  ip_hdr->ihl = 5;
  ip_hdr->version = 4;
  ip_hdr->total_length =
          rte_cpu_to_be_16 ( sizeof ( struct rte_ipv4_hdr ) +
                             sizeof ( struct rte_udp_hdr ) + len );

  // no IP fragments
  ip_hdr->packet_id = 0;
  ip_hdr->fragment_offset = 0;

  ip_hdr->time_to_live = 64;
  ip_hdr->next_proto_id = IPPROTO_UDP;
  ip_hdr->hdr_checksum = 0;
  ip_hdr->src_addr = rte_cpu_to_be_32 ( MAKE_IP_ADDR ( 10, 0, 0, 1 ) );
  ip_hdr->dst_addr = rte_cpu_to_be_32 ( MAKE_IP_ADDR ( 10, 0, 0, 2 ) );

  offset += sizeof ( struct rte_ipv4_hdr );
  struct rte_udp_hdr *udp_hdr;
  udp_hdr = rte_pktmbuf_mtod_offset ( pkt, struct rte_udp_hdr *, offset );
  udp_hdr->src_port = rte_cpu_to_be_16 ( 80 );
  udp_hdr->dst_port = rte_cpu_to_be_16 ( 1024 );
  udp_hdr->dgram_len = rte_cpu_to_be_16 ( sizeof ( struct rte_udp_hdr ) + len );
  udp_hdr->dgram_cksum = 0;

  uint8_t *payload = ( uint8_t * ) udp_hdr + sizeof ( struct rte_udp_hdr );
  rte_memcpy ( payload, buff, len );

  return rte_eth_tx_burst ( 0, ctx->queue, &pkt, 1 );
}

// simule application code...
void
app ( afp_ctx_t *ctx, void *arg )
{
  uint8_t buff[2048];

  uint8_t response[] = "Hi DPDK\n";

  while ( 1 )
    {
      size_t len = afp_recv ( ctx, buff, sizeof ( buff ) );

      if ( len )
        {
          DEBUG ( "App receveid pkt with lenght %u\n", len );
          DEBUG_ARRAY ( buff, len );

          // len = afp_send ( ctx, response, sizeof ( response ) );
          len = afp_send ( ctx, buff, len );
          DEBUG ( "Packets sent %u\n", len );
        }
    }
}

static int
worker ( void *arg )
{
  uint16_t queue = ( uint16_t ) ( uintptr_t ) arg;

  INFO ( "Core %u receiving packets on queue %d.\n", rte_lcore_id (), queue );

  afp_ctx_t ctx = { .queue = queue };
  app ( &ctx, arg );

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

  INFO ( "Cores available %d. Main core %d.\n",
         rte_lcore_count (),
         rte_get_main_lcore () );

  rte_eth_macaddr_get ( 0, &my_ether );

  pkt_pool = dpdk_init ( rte_lcore_count () - 1 );
  dataplane ();

  /* clean up the EAL */
  rte_eal_cleanup ();

  return 0;
}
