

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>

#include <unistd.h>
#include <signal.h>
#include <x86intrin.h>

#include <sys/syscall.h>

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#define gettid() syscall ( SYS_gettid )
#define tgkill( pid, tid, sig ) syscall ( SYS_tgkill, pid, tid, sig )
#endif

#include "util.h"

#define SENDER_CORE 5
#define WORKER_CORE 7

#define RUNS 100000UL

static volatile int worker_ready, worker_stop;
static uint32_t cycles_by_us;
static uint32_t samples[RUNS], timer_frequency[RUNS];
static uint32_t tsc_worker[RUNS], tsc_sender_worker[RUNS];
static uint64_t tsc_starts[RUNS], tsc_ends[RUNS];
static uint64_t volatile sender_start_tsc, worker_start_tsc;

static volatile int wait_handler = 1;

static volatile uint32_t i_handler = 0;

static pid_t worker_id;

static void
handler ( int sig )
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

static void *
worker ( void *arg )
{
  pin_to_cpu ( WORKER_CORE );
  printf ( "Started worker thread on core %u\n", sched_getcpu () );

  worker_id = gettid ();

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

  pid_t tgid = getpid ();

  for ( unsigned int i = 0; i < RUNS; i++ )
    {
      tsc_starts[i] = __rdtsc ();
      sender_start_tsc = tsc_starts[i];

      tgkill ( tgid, worker_id, SIGUSR1 );

      tsc_ends[i] = __rdtsc ();

      while ( wait_handler )
        asm volatile( "pause" );

      wait_handler = 1;
    }

  worker_stop = 1;

  for ( unsigned int i = 0; i < RUNS; i++ )
    samples[i] = tsc_ends[i] - tsc_starts[i];

  print ( samples, RUNS, "Sender", 0 );
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

  struct sigaction act = { .sa_handler = handler, .sa_flags = SA_NODEFER };
  sigaction ( SIGUSR1, &act, NULL );

  pthread_t tid;
  pthread_create ( &tid, NULL, worker, NULL );

  while ( !worker_ready )
    ;

  sender ();

  pthread_join ( tid, NULL );
  printf ( "\nSignals handled: %d\n", i_handler );

  return 0;
}
