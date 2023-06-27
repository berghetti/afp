
#define _GNU_SOURCE  // gettid

#include <sys/ucontext.h>
#include <ucontext.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

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

// statistics
// TODO: remove from here
static uint64_t swaps, interruptions, int_no_swaps, invalid_interruptions,
        yields;

static struct config conf = { .port_id = 0 };

static struct queue rxqs[MAX_WORKERS];

static struct rte_ring *wait_queue;
static uint16_t tot_workers;

static __thread uint16_t worker_id;

static __thread uint16_t hwq;  // hardware queue

static __thread ucontext_t main_ctx;
static __thread ucontext_t *worker_app_ctx;
static __thread ucontext_t *tmp_long_ctx = NULL;

static __thread afp_ctx_t ctx;

void
psp_server ( afp_ctx_t *ctx )
{
#define SHORT 1
#define LONG 2

  void *data;
  char *p;
  uint16_t len;
  struct sock sock;

  uint32_t id, type, nloop;
  size_t t;
  while ( 1 )
    {
      if ( !( t = afp_recv ( ctx, &data, &len, &sock ) ) )
        continue;

      // INFO ( "Reveived len %u\n", len );

      p = data;
      id = *( uint32_t * ) p;

      p += sizeof ( uint32_t );
      type = *( uint32_t * ) p;

      p += sizeof ( uint32_t ) * 2;
      nloop = *( uint32_t * ) p * 2.2f;

      // INFO ( "ID: %u TYPE: %u NLOOP: %u\n", id, type, nloop );

      if ( type == LONG )
        afp_send_feedback ( ctx, START_LONG );

      for ( unsigned i = 0; i < nloop; i++ )
        asm volatile( "nop" );

      if ( type == LONG )
        afp_send_feedback ( ctx, FINISHED_LONG );

      if ( !afp_send ( ctx, data, len, &sock ) )
        FATAL ( "%s\n", "Error to send packet" );
    }
}

static void
wrapper_app ( uint32_t msb_afp, uint32_t lsb_afp )
{
  afp_ctx_t *ctx = ( afp_ctx_t * ) ( ( uintptr_t ) msb_afp << 32 | lsb_afp );

  psp_server ( ctx );
}

static void
statistics ( int __notused sig )
{
  INFO ( "Statistics:\n"
         "  Interruptions:     %lu\n"
         "  Context swapts:    %lu\n"
         "  Long continue:     %lu\n"
         "  Invalid interruptions: %lu\n"
         "  yields:                %lu\n",
         interruptions,
         swaps,
         int_no_swaps,
         invalid_interruptions,
         yields );

  exit ( 0 );
}

// TODO: signal or dune interrupt or ipi module
void
interrupt_handler ( int __notused sig )
{
  interruptions++;

  // DEBUG ( "Worker %u received interrupt\n", worker_id );

  if ( !ctx.in_long_request )
    {
      invalid_interruptions++;
      return;
    }

  // Only get here when received a long request.
  // If not more work, continue current request
  if ( !has_work_in_queues ( &rxqs[worker_id], hwq ) )
    {
      int_no_swaps++;
      // DEBUG ( "Worker %u restart timer\n", worker_id );
      timer_set ( worker_id );
      // afp_send_feedback ( &ctx, START_LONG );
      return;
    }

  // enqueue long request
  // DEBUG ( "enqueue ctx %p\n", worker_app_ctx );
  if ( rte_ring_mp_enqueue ( wait_queue, worker_app_ctx ) )
    FATAL ( "%s\n",
            "Error enqueue long request. This is an memory leak and the "
            "request is lost.\n"
            "Try increase len wait_queue.\n" );

  // DEBUG ( "Worker %u return from interrupt\n", worker_id );

  // save worker_app_ctx and jump to main_ctx, that will create new app
  // context. next time worker_app_ctx run this starting after swapcontext
  // call and return to original app code
  swapcontext ( worker_app_ctx, &main_ctx );
  // worker_set_handler_status ( ctx.worker_id, false );
}

void
exit_to_context ( ucontext_t *long_ctx )
{
  DEBUG ( "Worker %u swap to long request %p\n", worker_id, long_ctx );

  yields++;

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

  // afp_ctx_t ctx = { .worker_id = worker_id,
  ctx = ( afp_ctx_t ){ .worker_id = worker_id,
                       .tot_workers = tot_workers,
                       .hwq = hwq,
                       .rxq = &rxqs[worker_id],
                       .rxqs = rxqs,
                       .wait_queue = wait_queue };

  // interrupt_register_work_tid ( worker_id, gettid () );
  interrupt_register_worker ( worker_id, rte_lcore_id () );

  queue_init ( ctx.rxq );

  void *ctxp = &ctx;
  uint32_t msb = ( ( uintptr_t ) ctxp >> 32 );
  uint32_t lsb = ( uint32_t ) ( uintptr_t ) ctxp;

  // context management
  while ( 1 )
    {
      DEBUG ( "Creating context on worker %u\n", worker_id );

      worker_app_ctx = context_alloc ();
      if ( !worker_app_ctx )
        FATAL ( "%s\n", "Error allocate context worker" );

      context_setlink ( worker_app_ctx, &main_ctx );
      worker_app_ctx->uc_link = &main_ctx;
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
          // context_setlink ( worker_app_ctx, &main_ctx );

          // known long request, rearming alarm.
          // asm volatile( "mfence" ::: "memory" );

          // afp_send_feedback ( &ctx, START_LONG );
          timer_set ( worker_id );

          swaps++;

          DEBUG ( "Going to long ctx: %p\n", worker_app_ctx );
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
  wait_queue = rte_ring_create ( "wait_queue",
                                 WAIT_QUEUE_SIZE,
                                 rte_socket_id (),
                                 0 );
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
  workers_init ();

  signal ( SIGINT, statistics );

  timer_main ( tot_workers );

  rte_ring_free ( wait_queue );
  /* clean up the EAL */
  rte_eal_cleanup ();

  return 0;
}
