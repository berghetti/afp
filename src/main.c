
#include <errno.h>
#include <generic/rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_mbuf_core.h>
#include <rte_memcpy.h>
#include <rte_mempool.h>
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
#include <sys/types.h>

#include "dpdk.h"
#include "debug.h"
#include "afp_netio.h"
#include "queue.h"

#include <unistd.h>

#define MAX_WORKERS 32

static struct config conf = { .port_id = 0 };

static struct queue rxqs[MAX_WORKERS];
static uint16_t hwqs[MAX_WORKERS];

static uint32_t tot_workers;

// simule application code...
void
app ( afp_ctx_t *ctx, void *arg )
{
  void *data;
  uint16_t len;
  struct sock sock;

  while ( 1 )
    {
      if ( afp_recv ( ctx, &data, &len, &sock ) )
        {
          // DEBUG ( "App received packet with size %u\n", len );
          // DEBUG ( "%s\n", ( char * ) data );
          //
          rte_delay_us ( 500 );

          char buff[] = "Hi DPDK!\n";
          afp_send ( ctx, buff, sizeof ( buff ), &sock );
        }
    }
}

static int
worker ( void *arg )
{
  uint16_t worker_id = ( uint16_t ) ( uintptr_t ) arg;

  queue_init ( &rxqs[worker_id] );
  INFO ( "Starting worker %u on core %u and queue %u.\n",
         worker_id,
         rte_lcore_id (),
         hwqs[worker_id] );

  afp_ctx_t ctx = { .worker_id = worker_id,
                    .tot_workers = tot_workers,
                    .queue = hwqs[worker_id],
                    .rxq = &rxqs[worker_id],
                    .rxqs = rxqs };

  // aqui preciso criar um contexto antes de chamar a função da aplicação
  while ( 1 )
    {
      app ( &ctx, arg );
    }

  return 0;
}

static void
dataplane ( struct config *conf )
{
  if ( rte_eth_dev_socket_id ( conf->port_id ) >= 0 &&
       rte_eth_dev_socket_id ( conf->port_id ) != ( int ) rte_socket_id () )
    WARNING ( "port %u is on remote NUMA node to "
              "polling thread.\n\tPerformance will "
              "not be optimal.\n",
              conf->port_id );

  unsigned int core_id;
  uint16_t worker_id = 0;
  RTE_LCORE_FOREACH_WORKER ( core_id )
  {
    if ( rte_eal_remote_launch ( worker,
                                 ( void * ) ( uintptr_t ) worker_id++,
                                 core_id ) )
      FATAL ( "Error start worker on core id %d: %s",
              core_id,
              rte_strerror ( rte_errno ) );
  }
}

// this functions run code of timer core.
// Now timer core is the main DPDK core
static void
core_timer ( void )
{

  while ( 1 )
    ;
}

// initialize hardware queues indexes
void
hwq_init ( int size )
{
  for ( int i = 0; i < size; i++ )
    hwqs[i] = i;
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

  rte_eth_macaddr_get ( 0, &conf.my_ether_addr );

  conf.num_queues = rte_lcore_count () - 1;

  tot_workers = conf.num_queues;
  hwq_init ( tot_workers );

  // conf.mbuf_pool = dpdk_init ( conf.num_queues );
  afp_netio_init ( &conf );
  dataplane ( &conf );

  DEBUG ( "%s\n", "Main core going to timer function" );
  core_timer ();

  /* clean up the EAL */
  rte_eal_cleanup ();

  return 0;
}
