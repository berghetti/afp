
/* Test overhead to send signals on same time from various threads.
 * Kernel taken lock to send signal, so us test misalign between threads
 * to send the signal to avoid taken the lock on same time.
 *
 * The 'us' parameter is the difference between threads starting o signal send.
 *
 */

#define _GNU_SOURCE  // to CPU_SET
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include <x86intrin.h>

#include "util.h"

#define RUNS 100000UL
#define MAX_WORKERS 32

static int us;
static int num_threads;
static volatile int ready, worker_stop;

static pid_t tids[MAX_WORKERS];

static uint32_t cycles_by_us;

static uint32_t samples[RUNS];
static uint32_t tsc_worker[RUNS];
static uint64_t tsc_starts[RUNS], tsc_ends[RUNS];

static __thread int worker_id;
static uint64_t start_tsc;

static uint32_t i = 0;
static void
handler ( int sig )
{
  // printf ( "Receiver(%u) %llu\n", worker_id, __rdtsc () );
  tsc_worker[i++] = __rdtsc () - start_tsc;
}

static inline double
cycles2us ( uint32_t cycles )
{
  return ( double ) cycles / cycles_by_us;
}

static void
print ( uint32_t *samples, char *msg )
{
  qsort ( samples, RUNS, sizeof ( samples[0] ), cmp_uint32 );

  uint32_t min, mean, p99, p999, max;
  min = samples[0];
  mean = percentile ( samples, RUNS, 0.50f );
  p99 = percentile ( samples, RUNS, 0.99f );
  p999 = percentile ( samples, RUNS, 0.999f );
  max = samples[RUNS - 1];

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
  worker_id = ( int ) ( uintptr_t ) arg;

  cpu_set_t cpuset;
  CPU_ZERO ( &cpuset );
  CPU_SET ( worker_id, &cpuset );
  if ( pthread_setaffinity_np ( pthread_self (),
                                sizeof ( cpu_set_t ),
                                &cpuset ) )
    {
      fprintf ( stderr,
                "Error to affinity thread %d to core %d\n",
                worker_id,
                worker_id );
      exit ( 1 );
    }

  tids[worker_id] = gettid ();
  // tids[worker_id] = pthread_self ();
  ready++;
  printf ( "Started worker thread %u on core %u\n", worker_id, worker_id );

  while ( !worker_stop )
    start_tsc = __rdtsc ();

  // print ( samples_worker, "Worker" );

  return 0;
}

static void
sender ( int i )
{
  cpu_set_t cpuset;
  CPU_ZERO ( &cpuset );
  CPU_SET ( i, &cpuset );
  if ( pthread_setaffinity_np ( pthread_self (),
                                sizeof ( cpu_set_t ),
                                &cpuset ) )
    {
      fprintf ( stderr, "Error to affinity thread %d to core %d\n", i, i );
      exit ( 1 );
    }

  pid_t tgid = getpid ();
  while ( ready < num_threads )
    ;

  printf ( "Started sender thread %u on core %u\n", i, i );

  for ( unsigned int i = 0; i < RUNS; i++ )
    {
      tsc_starts[i] = __rdtsc ();

      tgkill ( tgid, tids[1], SIGUSR1 );
      // pthread_kill ( tids[1], SIGUSR1 );

      tsc_ends[i] = __rdtsc ();

      uint64_t wait = tsc_ends[i] + us * cycles_by_us;
      while ( __rdtsc () < wait )
        __asm__ volatile( "pause" );
    }

  worker_stop = 1;

  for ( unsigned int i = 0; i < RUNS; i++ )
    samples[i] = tsc_ends[i] - tsc_starts[i];

  print ( samples, "Sender" );

  // for ( unsigned int i = 0; i < RUNS; i++ )
  //  samples[i] = tsc_worker[i] - tsc_starts[i];

  print ( tsc_worker, "Worker" );
}

/*
 * argv[1]: num threads to sending signal
 * argv[2]: microseconds interval misalign between threads to send signal
 */
int
main ( int argc, char **argv )
{
  if ( argc != 3 )
    {
      fprintf ( stderr, "Usage %s num_threads us\n", argv[0] );
      return 1;
    }

  num_threads = atoi ( argv[1] );

  us = atoi ( argv[2] );

  cycles_by_us = get_tsc_freq () / 1000000;

  struct sigaction act = { .sa_handler = handler };
  sigaction ( SIGUSR1, &act, NULL );

  printf ( "Threads:  %u\n"
           "Microseconds: %u\n"
           "Samples: %lu\n"
           "Cycles by us: %u\n",
           num_threads,
           us,
           RUNS,
           cycles_by_us );

  pthread_t ptids[num_threads];
  for ( int i = 0; i < num_threads; i++ )
    if ( pthread_create ( &ptids[i],
                          NULL,
                          worker,
                          ( void * ) ( uintptr_t ) i + 1 ) )
      return 1;

  sender ( 0 );

  for ( int i = 0; i < num_threads; i++ )
    pthread_join ( ptids[i], NULL );

  return 0;
}
