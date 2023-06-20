
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <x86intrin.h>

#include "util.h"
#include "trap.h"
#include "kmod_ipi.h"

// ensure not using hyperthreads of same core of workers
#define SENDER_CORE 3

// mapping between worker and cpu cores
#define WORKERS_CORES \
  {                   \
    4, 5, 6, 7        \
  }

#define NUM_WORKERS 4

#define RUNS 100000UL

#define WAIT 0
#define NOWAIT 1

static int workers_core[NUM_WORKERS] = WORKERS_CORES;

static uint64_t volatile sender_start_tsc[NUM_WORKERS];
static int volatile wait_handler[NUM_WORKERS];

static volatile bool worker_ready, workers_stop;

static uint32_t tsc_sender[RUNS];

static __thread uint64_t volatile worker_start_tsc;
static __thread uint32_t tsc_worker[RUNS];
static __thread uint32_t tsc_sender_worker[RUNS];
static __thread uint32_t timer_frequency[RUNS];
static __thread int worker_id;
static __thread uint32_t volatile i_handler = 0;

// static uint64_t volatile sender_start_tsc;

void
jmp_for_me ( void )
{
  static uint64_t last_now = 0;
  uint64_t now = __rdtsc ();

  // printf ( "here %u\n", worker_id );

  tsc_sender_worker[i_handler] = now - sender_start_tsc[worker_id];
  tsc_worker[i_handler] = now - worker_start_tsc;

  timer_frequency[i_handler] = now - last_now;
  last_now = now;

  i_handler++;

  wait_handler[worker_id] = NOWAIT;
}

struct worker_arg
{
  int id, core;
};

void *
worker ( void *arg )
{

  struct worker_arg *wa = ( struct worker_arg * ) arg;
  pin_to_cpu ( wa->core );
  printf ( "Started worker thread %u on core %u\n", wa->id, sched_getcpu () );

  worker_id = wa->id;

  worker_start_tsc = __rdtsc ();
  worker_ready = true;

  while ( !workers_stop )
    worker_start_tsc = __rdtsc ();

  char msg[64];

  snprintf ( msg, sizeof ( msg ), "Thread %u: sender/worker", worker_id );
  print ( tsc_sender_worker, i_handler, msg, 0 );

  snprintf ( msg, sizeof ( msg ), "Thread %u: worker", worker_id );
  print ( tsc_worker, i_handler, msg, 0 );

  snprintf ( msg, sizeof ( msg ), "Thread %u: timer frequency", worker_id );
  print ( timer_frequency, i_handler, msg, 1 );

  return NULL;
}

static void
sender ( int fd )
{
  pin_to_cpu ( SENDER_CORE );
  printf ( "Started sender thread on core %u\n", sched_getcpu () );

  uint64_t now;
  struct req_ipi req = { ._trap_entry = _trap_entry };

  unsigned i, j;
  for ( i = 0; i < RUNS; i++ )
    {
      // mensure latency to send to all workers
      now = __rdtsc ();
      for ( j = 0; j < NUM_WORKERS; j++ )
        {
          req.core = workers_core[j];
          sender_start_tsc[j] = __rdtsc ();

          ioctl ( fd, KMOD_IPI_SEND, &req );
        }
      tsc_sender[i] = __rdtsc () - now;

      // wait all workers finish interrupt handler
      for ( j = 0; j < NUM_WORKERS; j++ )
        {
          while ( wait_handler[j] == WAIT )
            asm volatile( "pause" );

          wait_handler[j] = WAIT;
        }
    }

  workers_stop = true;

  for ( unsigned i = 0; i < RUNS; i++ )
    tsc_sender[i] /= NUM_WORKERS;

  print ( tsc_sender, RUNS, "Sender per worker", 0 );
}

int
main ( int argc, char **argv )
{
  init_util ();

  printf ( "Samples: %lu\n"
           "Cycles by us: %u\n",
           RUNS,
           cycles_by_us );

  int fd = open ( KMOD_IPI_PATH, O_RDWR );
  if ( fd < 0 )
    {
      perror ( "Error to open character " KMOD_IPI_PATH );
      return 1;
    }

  unsigned i;
  pthread_t tids[NUM_WORKERS];
  struct worker_arg wa;
  for ( i = 0; i < NUM_WORKERS; i++ )
    {
      wa.id = i;
      wa.core = workers_core[i];
      pthread_create ( &tids[i], NULL, worker, &wa );

      while ( !worker_ready )
        ;

      worker_ready = false;
    }

  sender ( fd );
  close ( fd );

  for ( i = 0; i < NUM_WORKERS; i++ )
    pthread_join ( tids[i], NULL );

  return 0;
}
