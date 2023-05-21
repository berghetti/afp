
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
#include <rte_ring.h>
#include <rte_udp.h>
#include <stdbool.h>
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
#include <rte_malloc.h>
#include <rte_per_lcore.h>
#include <sys/types.h>

#include <sys/ucontext.h>
#include <ucontext.h>

#include "dpdk.h"
#include "debug.h"
#include "afp_netio.h"
#include "queue.h"
#include "context.h"

#define MAX_WORKERS 32

static struct config conf = { .port_id = 0 };

static struct queue rxqs[MAX_WORKERS];
static uint16_t hwqs[MAX_WORKERS];

static struct rte_ring *wait_queue;

static uint32_t tot_workers;
static __thread uint16_t worker_id;

static __thread ucontext_t main_ctx;
static __thread ucontext_t *worker_app_ctx;

// only worker_app_ctx turn this true
static __thread bool check_wait_queue = false;

// simule application code...
void
app ( afp_ctx_t *ctx )
{
  void *data;
  uint16_t len;
  struct sock sock;

  char buff[] = "Hi DPDK!\n";

  while ( 1 )
    {
      if ( afp_recv ( ctx, &data, &len, &sock ) )
        {
          // DEBUG ( "App received packet with size %u\n", len );
          // DEBUG ( "%s\n", ( char * ) data );

          rte_delay_us ( 500 );

          afp_send ( ctx, buff, sizeof ( buff ), &sock );
        }
    }
}

static void
wrapper_app ( uint32_t msb_afp, uint32_t lsb_afp )
{
  afp_ctx_t *ctx = ( afp_ctx_t * ) ( ( uintptr_t ) msb_afp << 32 | lsb_afp );

  app ( ctx );
}

// aqui preciso checar se a requisição atual continua o processamento.
// se não, coloca na wait queue e inicia um novo contexto.
// Para criar o novo contexto posso restaurar o main_ctx que vai cair no loop
// para criar um novo contexto. para colocar o contexto atual na wait queue
// preciso checar se é signal safe. Tambem tem que ver como verificar a wait
// queue depois.
//
// se sim, apenas retorna do handler;
// TODO: signal or dune interrupt
static void
interrupt_handler ( int sig )
{
  // If not more work continue current request
  if ( !has_work_in_queues ( &rxqs[worker_id], hwqs[worker_id] ) )
    return;

  // enqueue long request
  if ( rte_ring_mp_enqueue ( wait_queue, worker_app_ctx ) )
    ERROR ( "%s\n",
            "Error enqueue long request. This is an memory leak and the "
            "request is lost." );

  // save worker_app_ctx and jump to main_ctx, that will create new app context.
  // next time worker_app_ctx run this starting after swapcontext call
  // and return to original app code
  swapcontext ( worker_app_ctx, &main_ctx );
}

void
exit_context ( void )
{
  check_wait_queue = true;
  setcontext ( &main_ctx );
}

static int
worker ( void *arg )
{
  worker_id = ( uint16_t ) ( uintptr_t ) arg;

  if ( rte_eth_dev_socket_id ( 0 ) >= 0 &&
       rte_eth_dev_socket_id ( 0 ) != ( int ) rte_socket_id () )
    WARNING ( "port %u is on remote NUMA node to "
              "polling thread.\n\tPerformance will "
              "not be optimal.\n",
              0 );
  INFO ( "Starting worker %u on core %u and queue %u.\n",
         worker_id,
         rte_lcore_id (),
         hwqs[worker_id] );

  afp_ctx_t ctx = { .worker_id = worker_id,
                    .tot_workers = tot_workers,
                    .hwq = hwqs[worker_id],
                    .rxq = &rxqs[worker_id],
                    .rxqs = rxqs,
                    .wait_queue = wait_queue };

  queue_init ( ctx.rxq );

  void *ctxp = &ctx;
  uint32_t msb = ( ( uintptr_t ) ctxp >> 32 );
  uint32_t lsb = ( uint32_t ) ( uintptr_t ) ctxp;

  // aqui preciso criar um contexto antes de chamar a função da aplicação
  while ( 1 )
    {
      DEBUG ( "Creating context on worker %u\n", worker_id );
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

      // only main_ctx come here
      if ( check_wait_queue )
        {
          check_wait_queue = false;

          // TODO: replace to mempool
          rte_free ( worker_app_ctx->uc_stack.ss_sp );
          rte_free ( worker_app_ctx );

          if ( !rte_ring_mc_dequeue ( wait_queue,
                                      ( void ** ) &worker_app_ctx ) )
            {
              // current link can be from another thread, update this.
              context_setlink ( worker_app_ctx, &main_ctx );
              setcontext ( worker_app_ctx );
            }
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

// this functions run code of timer core.
// timer core is the main DPDK core
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

  if ( rte_lcore_count () < 3 )
    ERROR ( "%s", "Minimmum of cores should be 3" );

  INFO ( "Cores available %d. Main core %d.\n",
         rte_lcore_count (),
         rte_get_main_lcore () );

  rte_eth_macaddr_get ( 0, &conf.my_ether_addr );

  conf.num_queues = rte_lcore_count () - 1;
  tot_workers = conf.num_queues;

  hwq_init ( tot_workers );
  wait_queue_init ();
  afp_netio_init ( &conf );

  workers_init ();

  DEBUG ( "%s\n", "Main core going to timer function" );
  core_timer ();

  rte_ring_free ( wait_queue );
  /* clean up the EAL */
  rte_eal_cleanup ();

  return 0;
}
