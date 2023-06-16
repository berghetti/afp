
/* Test overhead to send signals on same time from various threads.
 * Kernel taken lock to send signal, so us test misalign between threads
 * to send the signal to avoid taken the lock on same time.
 *
 * The 'us' parameter is the difference between threads starting o signal send.
 *
 */

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

//#include <linux/signal.h> /* Definition of SI_* constants */
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <unistd.h>
#include <signal.h>
#include <x86intrin.h>

#include "util.h"

#define SENDER_CORE 0
#define WORKER_CORE 1

#define RUNS 100000UL

static volatile int worker_ready, worker_stop;
static uint32_t cycles_by_us, us;
static uint32_t samples[RUNS];
static uint32_t tsc_worker[RUNS], tsc_sender_worker[RUNS];
static uint64_t tsc_starts[RUNS], tsc_ends[RUNS];
static uint64_t volatile sender_start_tsc, worker_start_tsc;

static volatile uint32_t i_handler = 0;

static pid_t worker_id;

static void
handler ( int sig )
{
  // usleep ( 5 );
  uint64_t now = __rdtsc ();
  tsc_sender_worker[i_handler] = now - sender_start_tsc;
  tsc_worker[i_handler] = now - worker_start_tsc;

  i_handler++;
}

static inline double
cycles2us ( uint32_t cycles )
{
  return ( double ) cycles / cycles_by_us;
}

static void
print ( uint32_t *samples, size_t size, char *msg )
{
  qsort ( samples, size, sizeof ( *samples ), cmp_uint32 );

  uint32_t min, mean, p99, p999, max;
  min = samples[0];
  mean = percentile ( samples, size, 0.50f );
  p99 = percentile ( samples, size, 0.99f );
  p999 = percentile ( samples, size, 0.999f );
  max = samples[size - 1];

  printf ( "\n%s\n"
           "  Min:   %u (%.2f us)\n"
           "  50%%:   %u (%.2f us)\n"
           "  99%%:   %u (%.2f us)\n"
           "  99.9%%: %u (%.2f us)\n"
           "  Max:   %u (%.2f us)\n",
           msg,
           min,
           cycles2us ( min ),
           mean,
           cycles2us ( mean ),
           p99,
           cycles2us ( p99 ),
           p999,
           cycles2us ( p999 ),
           max,
           cycles2us ( max ) );
}

static void *
worker ( void *arg )
{
  cpu_set_t cpus;
  CPU_ZERO ( &cpus );
  CPU_SET ( WORKER_CORE, &cpus );
  if ( pthread_setaffinity_np ( pthread_self (), sizeof ( cpu_set_t ), &cpus ) )
    {
      printf ( "Could not pin thread to core %d.\n", WORKER_CORE );
      exit ( 1 );
    }

  printf ( "Started worker thread on core %u\n", WORKER_CORE );

  worker_id = gettid ();

  worker_ready = 1;
  while ( !worker_stop )
    worker_start_tsc = __rdtsc ();

  return NULL;
}

static void
sender ( void )
{
  cpu_set_t cpus;
  CPU_ZERO ( &cpus );
  CPU_SET ( SENDER_CORE, &cpus );
  if ( pthread_setaffinity_np ( pthread_self (), sizeof ( cpu_set_t ), &cpus ) )
    {
      printf ( "Could not pin thread to core %d.\n", SENDER_CORE );
      exit ( 1 );
    }

  printf ( "Started sender thread on core %u\n", SENDER_CORE );

  pid_t tgid = getpid ();
  for ( unsigned int i = 0; i < RUNS; i++ )
    {
      tsc_starts[i] = __rdtsc ();
      sender_start_tsc = tsc_starts[i];

      tgkill ( tgid, worker_id, SIGUSR1 );

      tsc_ends[i] = __rdtsc ();

      uint64_t wait = tsc_ends[i] + us * cycles_by_us;
      while ( __rdtsc () < wait )
        asm volatile( "pause" );
    }

  worker_stop = 1;

  for ( unsigned int i = 0; i < RUNS; i++ )
    samples[i] = tsc_ends[i] - tsc_starts[i];

  print ( samples, RUNS, "Sender" );
  print ( tsc_sender_worker, i_handler, "Sender/Worker" );
  print ( tsc_worker, i_handler, "Worker" );

  printf ( "\nSignals handled: %d\n", i_handler );
}

/*
 * argv[1]: num threads to sending signal
 * argv[2]: microseconds interval misalign between threads to send signal
 */
int
main ( int argc, char **argv )
{
  if ( argc != 2 )
    {
      fprintf ( stderr,
                "Usage: %s us\n'us' is time between interruptions\n",
                argv[0] );
      return 1;
    }

  cycles_by_us = get_tsc_freq () / 1000000;
  us = atoi ( argv[1] );

  printf ( "Microseconds: %u\n"
           "Samples: %lu\n"
           "Cycles by us: %u\n",
           us,
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

  return 0;
}
