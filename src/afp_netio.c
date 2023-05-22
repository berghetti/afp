
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "dpdk.h"
#include "queue.h"
#include "afp_netio.h"
#include "debug.h"
#include "interrupt.h"
#include "timer.h"
#include "compiler.h"

static uint16_t port_id;
// static struct rte_mempool *pkt_pool;
static struct rte_ether_addr my_ether;

// TODO: add this to a header file
extern void __noreturn
exit_to_context ( void * );

#define BURST_SIZE 32

#define SWAP( a, b )           \
  ( {                          \
    typeof ( a ) _tmp = ( a ); \
    ( a ) = ( b );             \
    ( b ) = _tmp;              \
  } )

#define MAKE_IP_ADDR( a, b, c, d )                        \
  ( ( ( uint32_t ) a << 24 ) | ( ( uint32_t ) b << 16 ) | \
    ( ( uint32_t ) c << 8 ) | ( uint32_t ) d )

#define MIN_PKT_SIZE                                     \
  ( RTE_ETHER_HDR_LEN + sizeof ( struct rte_ipv4_hdr ) + \
    sizeof ( struct rte_udp_hdr ) )

static int
parse_pkt ( struct rte_mbuf *pkt, void **payload, uint16_t *payload_size )
{
  struct rte_ether_hdr *eth;
  eth = rte_pktmbuf_mtod ( pkt, struct rte_ether_hdr * );
  if ( !rte_is_same_ether_addr ( &eth->dst_addr, &my_ether ) )
    {
      DEBUG ( "%s\n", "Not for me" );
      return -1;
    }

  if ( eth->ether_type != rte_cpu_to_be_16 ( RTE_ETHER_TYPE_IPV4 ) )
    {
      DEBUG ( "%s\n", "Not IPv4" );
      return -1;
    }

  uint32_t offset = sizeof ( struct rte_ether_hdr );
  struct rte_ipv4_hdr *ip;
  ip = rte_pktmbuf_mtod_offset ( pkt, struct rte_ipv4_hdr *, offset );

  if ( ( ip->next_proto_id != IPPROTO_UDP ) )
    {
      DEBUG ( "%s\n", "Not UDP" );
      return -1;
    }

  offset += sizeof ( struct rte_ipv4_hdr );
  struct rte_udp_hdr *udp =
          rte_pktmbuf_mtod_offset ( pkt, struct rte_udp_hdr *, offset );

  offset += sizeof ( struct rte_udp_hdr );
  *payload = rte_pktmbuf_mtod_offset ( pkt, void *, offset );
  *payload_size =
          rte_be_to_cpu_16 ( udp->dgram_len ) - sizeof ( struct rte_udp_hdr );

  return 0;
}

/*
 * div_up - divides two numbers, rounding up to an integer
 * @x: the dividend
 */
#define div_up( x, d ) ( ( ( ( x ) + ( d ) -1 ) ) / ( d ) )

// retorn amount work stolen
static uint32_t
work_stealing ( struct queue *my, struct queue *remote )
{
  if ( !queue_trylock ( remote ) )
    return 0;

  uint32_t size = queue_count ( remote );

  if ( size )
    {
      size = div_up ( size, 2 );

      DEBUG ( "Stealing %u packets\n", size );
      queue_stealing ( my, remote, size );
    }

  queue_unlock ( remote );
  return size;
}

bool
has_work_in_queues ( struct queue *rxq, uint16_t hwq )
{
  if ( !queue_is_empty ( rxq ) )
    return true;

  struct rte_mbuf *pkts[BURST_SIZE];
  uint16_t nb_rx;

  nb_rx = rte_eth_rx_burst ( port_id, hwq, pkts, BURST_SIZE );
  if ( nb_rx )
    {
      // DEBUG ( "Queue %u received %u packets\n", ctx->queue, nb_rx );
      // DEBUG ( "Worker %u received %u packets\n", ctx->worker_id, nb_rx );
      if ( !queue_enqueue_bulk ( rxq, ( void ** ) pkts, nb_rx ) )
        FATAL ( "%s\n", "Error enqueue bulk" );

      return true;
    }

  return false;
}

size_t
afp_recv ( afp_ctx_t *ctx, void **data, uint16_t *len, struct sock *s )
{
  struct rte_mbuf *pkt;
  queue_lock ( ctx->rxq );
  DEBUG ( "Worker: %u\n", ctx->worker_id );
  while ( 1 )
    {
      if ( has_work_in_queues ( ctx->rxq, ctx->hwq ) )
        goto done;

      void *long_ctx;
      if ( !rte_ring_mc_dequeue ( ctx->wait_queue, &long_ctx ) )
        {
          queue_unlock ( ctx->rxq );
          exit_to_context ( long_ctx );
        }

      static __thread uint16_t remote_worker = 0;
      for ( uint16_t i = 0; i < ctx->tot_workers; i++ )
        {
          remote_worker = ( remote_worker + 1 ) % ctx->tot_workers;

          if ( ctx->worker_id == remote_worker )
            continue;

          // DEBUG ( "Worker %u try stealing from %u\n",
          //        ctx->worker_id,
          //        remote_worker );
          if ( work_stealing ( ctx->rxq, &ctx->rxqs[remote_worker] ) )
            {
              DEBUG ( "Worker %u stealead %u packtes from worker %u\n",
                      ctx->worker_id,
                      queue_count ( ctx->rxq ),
                      remote_worker );
              goto done;
            }
        }
    }

done:
  pkt = queue_dequeue ( ctx->rxq );
  queue_unlock ( ctx->rxq );
  // DEBUG ( "Rxq size on worker %u: %u\n",
  //        ctx->worker_id,
  //        queue_count ( ctx->rxq ) );

  if ( parse_pkt ( pkt, data, len ) )
    {
      rte_pktmbuf_free ( pkt );
      return 0;
    }

  s->pkt = pkt;

  return 1;
}

ssize_t
afp_send ( afp_ctx_t *ctx, void *buff, uint16_t len, struct sock *s )
{
  // DEBUG ( "%s\n", "send pkt" );
  struct rte_mbuf *pkt = s->pkt;

  // offloading cksum to hardware
  pkt->ol_flags |= ( RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM |
                     RTE_MBUF_F_TX_UDP_CKSUM );

  struct rte_ether_hdr *eth_hdr;
  eth_hdr = rte_pktmbuf_mtod ( pkt, struct rte_ether_hdr * );
  eth_hdr->dst_addr = eth_hdr->src_addr;
  eth_hdr->src_addr = my_ether;

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
  // ip_hdr->src_addr = rte_cpu_to_be_32 ( MAKE_IP_ADDR ( 10, 0, 0, 1 ) );
  // ip_hdr->dst_addr = rte_cpu_to_be_32 ( MAKE_IP_ADDR ( 10, 0, 0, 2 ) );

  SWAP ( ip_hdr->src_addr, ip_hdr->dst_addr );

  offset += sizeof ( struct rte_ipv4_hdr );
  struct rte_udp_hdr *udp_hdr;
  udp_hdr = rte_pktmbuf_mtod_offset ( pkt, struct rte_udp_hdr *, offset );
  // udp_hdr->src_port = rte_cpu_to_be_16 ( 80 );
  // udp_hdr->dst_port = rte_cpu_to_be_16 ( 1024 );
  SWAP ( udp_hdr->src_port, udp_hdr->dst_port );

  udp_hdr->dgram_len = rte_cpu_to_be_16 ( sizeof ( struct rte_udp_hdr ) + len );
  udp_hdr->dgram_cksum = 0;

  offset += sizeof ( struct rte_udp_hdr );
  void *payload = rte_pktmbuf_mtod_offset ( pkt, void *, offset );
  rte_memcpy ( payload, buff, len );

  // TODO: need this?
  pkt->l2_len = RTE_ETHER_HDR_LEN;
  pkt->l3_len = sizeof ( struct rte_ipv4_hdr );
  pkt->l4_len = sizeof ( struct rte_udp_hdr );

  // Total size packet
  pkt->pkt_len = pkt->data_len = MIN_PKT_SIZE + len;

  return rte_eth_tx_burst ( port_id, ctx->hwq, &pkt, 1 );
}

void
afp_netio_init ( struct config *conf )
{
  dpdk_init ( conf->port_id, conf->num_queues );
  port_id = conf->port_id;
  my_ether = conf->my_ether_addr;
}
