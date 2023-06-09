
#define _GNU_SOURCE  // gettid


#include <sys/ucontext.h>
#include <ucontext.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <signal.h>

#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "globals.h"
#include "dpdk.h"
#include "debug.h"
#include "afp_netio.h"
#include "queue.h"
#include "context.h"
#include "interrupt.h"
#include "timer.h"
#include "afp_internal.h"
#include "feedback.h"
#include "stats.h"
#include "swap.h"

static struct config conf = { .port_id = 0 };

static __thread ucontext_t main_ctx, *worker_app_ctx, *tmp_long_ctx;

void
psp_server ( void )
{
#define SHORT 1
#define LONG 2

  void *data;
  char *p;
  uint16_t len;

  struct sock sock;

  uint32_t id, type, nloop;
  while ( 1 )
    {
      if ( !afp_recv ( &data, &len, &sock ) )
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
        afp_send_feedback ( START_LONG );

      for ( unsigned i = 0; i < nloop; i++ )
        asm volatile( "nop" );

      if ( type == LONG )
        afp_send_feedback ( FINISHED_LONG );

      if ( !afp_send ( data, len, &sock ) )
        FATAL ( "%s\n", "Error to send packet" );
    }
}

static void
wrapper_app ( uint32_t msb_afp, uint32_t lsb_afp )
{
  void *ctx = ( void * ) ( ( uintptr_t ) msb_afp << 32 | lsb_afp );
  ctx = ctx;

  psp_server ();

  // TODO: check this
  /* if app code return, jump to main context.
   * this avoid update return adress on stack of app context when this change of
   * core */
  // context_free ( worker_app_ctx );
  setcontext ( &main_ctx );
}

// signal, dune or ipi module interrupt
void
interrupt_handler ( int __notused sig )
{
  interruptions++;

  DEBUG ( "Worker %u received interrupt\n", worker_id );

  /* invalid interrupt are when long request is finished but we yet receve a
   * interrupt. This could be a interrupt on bus */
  if ( rte_atomic16_read ( &in_long_request ) == 0 )
    {
      invalid_interruptions++;
      return;
    }

  /* Only get here when handling a long request.
   * If not more work, continue current request */
  if ( !afp_netio_has_work () )
    {
      int_no_swaps++;
      timer_tryset ( worker_id );
      return;
    }

  /* enqueue long request */
  if ( rte_ring_mp_enqueue ( wait_queue, worker_app_ctx ) )
    FATAL ( "%s\n",
            "Error enqueue long request.\n"
            "Try increase len wait_queue.\n" );

  /* save worker_app_ctx and jump to main_ctx, that will create new app
   * context. next time worker_app_ctx run this starting after swapcontext
   * call and return to original app code */
  swapcontext ( worker_app_ctx, &main_ctx );
}

void
exit_to_context ( ucontext_t *long_ctx )
{
  DEBUG ( "Worker %u swap to long request %p\n", worker_id, long_ctx );

  yields++;

  tmp_long_ctx = long_ctx;
  afp_setcontext ( &main_ctx );
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

  // interrupt_register_work_tid ( worker_id, gettid () );
  interrupt_register_worker ( worker_id, rte_lcore_id () );

  afp_netio_init_per_worker ();

  // TODO
  void *ctxp = NULL;
  uint32_t msb = ( ( uintptr_t ) ctxp >> 32 );
  uint32_t lsb = ( uint32_t ) ( uintptr_t ) ctxp;

  // context management
  while ( 1 )
    {
      DEBUG ( "Creating context on worker %u\n", worker_id );

      worker_app_ctx = context_alloc ();
      if ( !worker_app_ctx )
        FATAL ( "%s\n", "Error allocate context worker" );

      getcontext ( worker_app_ctx );
      worker_app_ctx->uc_link = &main_ctx;
      makecontext ( worker_app_ctx,
                    ( void ( * ) ( void ) ) wrapper_app,
                    2,
                    msb,
                    lsb );

      // context_setlink ( worker_app_ctx, &main_ctx );

      // go to app context
      swapcontext ( &main_ctx, worker_app_ctx );

      // swapt between context without work to long request context
      if ( tmp_long_ctx )
        {
          swaps++;
          context_free ( worker_app_ctx );
          worker_app_ctx = tmp_long_ctx;
          tmp_long_ctx = NULL;

          // update return address
          // context_setlink ( worker_app_ctx, &main_ctx );

          DEBUG ( "Going to long ctx: %p\n", worker_app_ctx );

          /* known long request, rearming alarm with delay to compensate
           * the delay until get app code */
          timer_set_delay ( worker_id, 20 );

          afp_setcontext ( worker_app_ctx );
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
    FATAL ( "%s\n", "Cannot init EAL\n" );

  if ( rte_lcore_count () < 2 )
    FATAL ( "%s", "Minimmum of cores should be 2" );

  INFO ( "Cores available %d. Main core %d.\n",
         rte_lcore_count (),
         rte_get_main_lcore () );

  rte_eth_macaddr_get ( 0, &conf.my_ether_addr );

  conf.num_queues = rte_lcore_count () - 1;
  tot_workers = conf.num_queues;

  /* inits */
  wait_queue_init ();
  afp_netio_init ( &conf );
  interrupt_init ( interrupt_handler );
  workers_init ();
  stats_init ();

  /* timer main */
  timer_main ( tot_workers );

  /* clean ups */
  rte_ring_free ( wait_queue );
  rte_eal_cleanup ();

  return 0;
}
