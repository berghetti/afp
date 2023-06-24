
#define _GNU_SOURCE  // gettid
#include <unistd.h>

#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "dpdk.h"
#include "debug.h"
#include "afp_netio.h"
#include "queue.h"
#include "context.h"
#include "interrupt.h"
#include "timer.h"
#include "afp_internal.h"
#include "afp.h"

static struct config conf = { .port_id = 0 };

static struct queue rxqs[MAX_WORKERS];

static struct rte_ring *wait_queue;
static uint16_t tot_workers;

static __thread uint16_t worker_id;

static __thread uint16_t hwq;  // hardware queue

static __thread ucontext_t main_ctx;
static __thread ucontext_t *worker_app_ctx;
static __thread ucontext_t *tmp_long_ctx = NULL;

// simule application code...
// void
// app ( afp_ctx_t *ctx )
//{
//  void *data;
//  uint16_t len;
//  struct sock sock;
//
//  char s[] = "Short response!\n";
//  char l[] = "LONG response!\n";
//
//  size_t t;
//  while ( 1 )
//    {
//      if ( !( t = afp_recv ( ctx, &data, &len, &sock ) ) )
//        continue;
//
//      if ( !strcmp ( data, "LONG\n" ) )
//        {
//          int i = 50;
//          DEBUG ( "Worker %u received long request\n", worker_id );
//          afp_send_feedback ( ctx, START_LONG );
//          while ( i-- )
//            {
//              int j = 100000000UL;
//              while ( j-- )
//                ;
//              DEBUG ( "long request: %u\n", i );
//            }
//
//          afp_send ( ctx, l, sizeof ( l ), &sock );
//          afp_send_feedback ( ctx, FINISHED_LONG );
//        }
//      else
//        {
//          DEBUG ( "Worker %u received short request\n", worker_id );
//          // DEBUG ( "App received packet with size %u\n", len );
//          // DEBUG ( "%s\n", ( char * ) data );
//
//          // rte_delay_ms ( 500 );
//
//          afp_send ( ctx, s, sizeof ( s ), &sock );
//        }
//    }
//}

void
psp_server ( afp_ctx_t *ctx )
{
  void *data;
  uint16_t len;
  struct sock sock;

  size_t t;
  while ( 1 )
    {
      if ( !( t = afp_recv ( ctx, &data, &len, &sock ) ) )
        continue;

      afp_send ( ctx, data, len, &sock );
    }
}

static void
wrapper_app ( uint32_t msb_afp, uint32_t lsb_afp )
{
  afp_ctx_t *ctx = ( afp_ctx_t * ) ( ( uintptr_t ) msb_afp << 32 | lsb_afp );

  psp_server ( ctx );
}

// TODO: signal or dune interrupt
static void
interrupt_handler ( int __notused sig )
{
  uint64_t now = rte_get_tsc_cycles ();
  // DEBUG ( "%lu: Worker %u received interrupt\n", now, worker_id );

  // Only get here when received a long request.
  // If not more work, continue current request
  if ( !has_work_in_queues ( &rxqs[worker_id], hwq ) )
    {
      // DEBUG ( "Worker %u restart timer\n", worker_id );
      timer_set ( worker_id, now );
      return;
    }

  // enqueue long request
  DEBUG ( "enqueue ctx %p\n", worker_app_ctx );
  if ( rte_ring_mp_enqueue ( wait_queue, worker_app_ctx ) )
    ERROR ( "%s\n",
            "Error enqueue long request. This is an memory leak and the "
            "request is lost." );

  DEBUG ( "Worker %u return from interrupt\n", worker_id );

  // save worker_app_ctx and jump to main_ctx, that will create new app
  // context. next time worker_app_ctx run this starting after swapcontext
  // call and return to original app code
  swapcontext ( worker_app_ctx, &main_ctx );
}

void
exit_to_context ( ucontext_t *long_ctx )
{
  DEBUG ( "Worker %u swap to long request %p\n", worker_id, long_ctx );

  tmp_long_ctx = long_ctx;
  setcontext ( &main_ctx );
}

static int
worker ( void *arg )
{
  worker_id = ( uint16_t ) ( uintptr_t ) arg;

  hwq = worker_id;

  if ( rte_eth_dev_socket_id ( conf.port_id ) >= 0 &&
       rte_eth_dev_socket_id ( conf.port_id ) != ( int ) rte_socket_id () )
    {
      WARNING ( "port %u is on remote NUMA node to "
                "polling thread.\n\tPerformance will "
                "not be optimal.\n",
                conf.port_id );
    }

  INFO ( "Starting worker %u on core %u (queue %u).\n",
         worker_id,
         rte_lcore_id (),
         hwq );

  afp_ctx_t ctx = { .worker_id = worker_id,
                    .tot_workers = tot_workers,
                    .hwq = hwq,
                    .rxq = &rxqs[worker_id],
                    .rxqs = rxqs,
                    .wait_queue = wait_queue };

  interrupt_register_work_tid ( worker_id, gettid () );

  queue_init ( ctx.rxq );

  void *ctxp = &ctx;
  uint32_t msb = ( ( uintptr_t ) ctxp >> 32 );
  uint32_t lsb = ( uint32_t ) ( uintptr_t ) ctxp;

  // context management
  while ( 1 )
    {
      // DEBUG ( "Creating context on worker %u\n", worker_id );

      worker_app_ctx = context_alloc ();
      if ( !worker_app_ctx )
        FATAL ( "%s\n", "Error allocate context worker" );

      context_setlink ( worker_app_ctx, &main_ctx );
      getcontext ( worker_app_ctx );
      makecontext ( worker_app_ctx,
                    ( void ( * ) ( void ) ) wrapper_app,
                    2,
                    msb,
                    lsb );

      // go to app context
      swapcontext ( &main_ctx, worker_app_ctx );

      // swapt between context without work to long request context
      if ( tmp_long_ctx )
        {
          context_free ( worker_app_ctx );
          worker_app_ctx = tmp_long_ctx;
          tmp_long_ctx = NULL;

          // update return address
          context_setlink ( worker_app_ctx, &main_ctx );

          // known long request, rearming alarm.
          timer_set ( worker_id, rte_get_tsc_cycles () );
          setcontext ( worker_app_ctx );
        }
    }

  return 0;
}

static void
workers_init ( void )
{
  uint16_t worker_id = 0;

  unsigned int core_id;
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

void
wait_queue_init ( void )
{
  // MC/MP queue
  wait_queue = rte_ring_create ( "wait_queue", 2048, rte_socket_id (), 0 );
  if ( !wait_queue )
    FATAL ( "%s\n", "Error allocate wait queue" );
}

int
main ( int argc, char **argv )
{
  int ret;

  ret = rte_eal_init ( argc, argv );
  if ( ret < 0 )
    rte_panic ( "Cannot init EAL\n" );

  if ( rte_lcore_count () < 2 )
    FATAL ( "%s", "Minimmum of cores should be 2" );

  INFO ( "Cores available %d. Main core %d.\n",
         rte_lcore_count (),
         rte_get_main_lcore () );

  rte_eth_macaddr_get ( 0, &conf.my_ether_addr );

  conf.num_queues = rte_lcore_count () - 1;
  tot_workers = conf.num_queues;

  // hwq_init ( tot_workers );
  wait_queue_init ();
  afp_netio_init ( &conf );

  interrupt_init ( interrupt_handler );
  timer_init ( tot_workers );
  workers_init ();

  timer_main ( tot_workers );

  rte_ring_free ( wait_queue );
  /* clean up the EAL */
  rte_eal_cleanup ();

  return 0;
}
