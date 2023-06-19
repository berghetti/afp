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
#include <sched.h>
#include <x86intrin.h>

#include "util.h"

#include "libdune/dune.h"
#include "libdune/cpu-x86.h"
#include "libdune/local.h"

#define SENDER_CORE 0
#define WORKER_CORE 1

#define RUNS 100000UL

#define TEST_VECTOR 0xf2

static volatile int worker_ready, worker_stop;

static uint32_t tsc_sender[RUNS], timer_frequency[RUNS];
static uint32_t tsc_worker[RUNS], tsc_sender_worker[RUNS];

static uint64_t volatile sender_start_tsc, worker_start_tsc;

static volatile int wait_handler = 1;

static volatile uint32_t i_handler = 0;

static void
jmp_for_me ( struct dune_tf *tf )
{
  static uint64_t last_now = 0;
  uint64_t now = __rdtsc ();

  tsc_sender_worker[i_handler] = now - sender_start_tsc;
  tsc_worker[i_handler] = now - worker_start_tsc;

  timer_frequency[i_handler] = now - last_now;
  last_now = now;

  i_handler++;

  dune_apic_eoi ();
  wait_handler = 0;
}

void *
worker ( void *arg )
{
  pin_to_cpu ( WORKER_CORE );
  printf ( "Started worker thread on core %u\n", sched_getcpu () );

  volatile int ret = dune_enter ();
  if ( ret )
    {
      fprintf ( stderr, "failed to enter dune in thread 2\n" );
      exit ( 1 );
    }

  dune_apic_init_rt_entry ();
  dune_register_intr_handler ( TEST_VECTOR, jmp_for_me );
  asm volatile( "mfence" ::: "memory" );

  worker_start_tsc = __rdtsc ();
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

      dune_apic_send_ipi ( TEST_VECTOR,
                           dune_apic_id_for_cpu ( WORKER_CORE, NULL ) );

      tsc_sender[i] = __rdtsc () - now;

      while ( wait_handler )
        asm volatile( "pause" );

      wait_handler = 1;
    }

  worker_stop = 1;

  print ( tsc_sender_worker, RUNS, "Sender", 0 );
  print ( tsc_sender_worker, RUNS, "Sender/Worker", 0 );
  print ( tsc_worker, RUNS, "Worker", 0 );
  print ( timer_frequency, RUNS, "Worker", 1 );
}

int
main ( int argc, char **argv )
{
  init_util ();

  printf ( "Samples: %lu\n"
           "Cycles by us: %u\n",
           RUNS,
           cycles_by_us );

  volatile int ret = dune_init_and_enter ();
  if ( ret )
    {
      fprintf ( stderr, "failed to initialize dune\n" );
      return ret;
    }

  dune_apic_init_rt_entry ();

  pthread_t tid;
  pthread_create ( &tid, NULL, worker, NULL );

  while ( !worker_ready )
    ;

  sender ();

  pthread_join ( tid, NULL );
  printf ( "\nDUNE IPIs handled: %d\n", i_handler );

  return 0;
}
