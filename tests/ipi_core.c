
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <x86intrin.h>

#include "util.h"
#include "trap.h"

// ensure not using hyperthreads of same core
#define SENDER_CORE 6
#define WORKER_CORE 7

#define RUNS 100000UL

static volatile int worker_ready, worker_stop;

static uint32_t tsc_sender[RUNS], timer_frequency[RUNS];
static uint32_t tsc_worker[RUNS], tsc_sender_worker[RUNS];

static uint64_t volatile sender_start_tsc, worker_start_tsc;

static volatile int wait_handler = 1;

static volatile uint32_t i_handler = 0;

void
jmp_for_me ( void )
{
  static uint64_t last_now = 0;
  uint64_t now = __rdtsc ();

  tsc_sender_worker[i_handler] = now - sender_start_tsc;
  tsc_worker[i_handler] = now - worker_start_tsc;

  timer_frequency[i_handler] = now - last_now;
  last_now = now;

  i_handler++;

  wait_handler = 0;
}

void *
worker ( void *arg )
{
  pin_to_cpu ( WORKER_CORE );
  printf ( "Started worker thread on core %u\n", sched_getcpu () );

  worker_ready = 1;
  while ( !worker_stop )
    worker_start_tsc = __rdtsc ();

  return NULL;
}

static void
sender ( void )
{
  pin_to_cpu ( SENDER_CORE );
  printf ( "Started sender thread on core %u\n", sched_getcpu () );

  uint64_t now;

  for ( unsigned int i = 0; i < RUNS; i++ )
    {
      now = __rdtsc ();
      sender_start_tsc = now;

      trap_send_interrupt ( WORKER_CORE );

      tsc_sender[i] = __rdtsc () - now;

      while ( wait_handler )
        asm volatile( "pause" );

      wait_handler = 1;
    }

  worker_stop = 1;

  print ( tsc_sender, RUNS, "Sender", 0 );
  print ( tsc_sender_worker, i_handler, "Sender/Worker", 0 );
  print ( tsc_worker, i_handler, "Worker", 0 );
  print ( timer_frequency, RUNS, "Timer frequency", 1 );
}

int
main ( int argc, char **argv )
{
  init_util ();

  printf ( "Samples: %lu\n"
           "Cycles by us: %u\n",
           RUNS,
           cycles_by_us );

  if ( trap_init ( jmp_for_me ) < 0 )
    return 1;

  pthread_t tid;
  pthread_create ( &tid, NULL, worker, NULL );

  while ( !worker_ready )
    ;

  sender ();
  pthread_join ( tid, NULL );
  trap_free ();

  printf ( "\nIPIs handled: %d\n", i_handler );
  return 0;
}
