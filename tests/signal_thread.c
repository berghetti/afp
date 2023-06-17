
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
#include <stdatomic.h>

#include <sys/syscall.h>

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#define gettid() syscall ( SYS_gettid )
#define tgkill( pid, tid, sig ) syscall ( SYS_tgkill, pid, tid, sig )
#endif

#include "util.h"

#define RUNS 100000UL

static int us;
static int num_threads;
static int ready;

static uint32_t cycles_by_us;

static struct timespec start;

static pthread_barrier_t barrier;

static __thread volatile uint64_t ts;
static __thread uint32_t samples[RUNS];

static void
handler ( int sig )
{
  static __thread uint64_t i = 0;
  samples[i++] = __rdtsc () - ts;
}

static inline double
cycles2us ( uint32_t cycles )
{
  return ( double ) cycles / cycles_by_us;
}

static void
print ( int thread_id )
{
  qsort ( samples, RUNS, sizeof ( samples[0] ), cmp_uint32 );

  uint32_t min, mean, p99, p999, max;
  min = samples[0];
  mean = percentile ( samples, RUNS, 0.50f );
  p99 = percentile ( samples, RUNS, 0.99f );
  p999 = percentile ( samples, RUNS, 0.999f );
  max = samples[RUNS - 1];

  printf ( "\nThread %d\n"
           "  Min:   %u (%.2f us)\n"
           "  Mean:  %u (%.2f us)\n"
           "  99%%:   %u (%.2f us)\n"
           "  99.9%%: %u (%.2f us)\n"
           "  Max:   %u (%.2f us)\n",
           thread_id,
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
f1 ( void *arg )
{
  int i = ( int ) ( uintptr_t ) arg;

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
  pid_t tid = gettid ();
  struct timespec now;
  uint64_t end;

  uint64_t count = RUNS;
  // First execution not await others threads. Assume as warm up.
  while ( count-- )
    {
      ts = __rdtsc ();
      tgkill ( tgid, tid, SIGUSR1 );

      // ensure specific misalign between threads start
      pthread_barrier_wait ( &barrier );

      uint64_t end = __rdtsc () + us * i * cycles_by_us;
      while ( __rdtsc () < end )
        ;
    }

  print ( i );

  return 0;
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
  num_threads += ( num_threads == 0 );

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

  pthread_barrier_init ( &barrier, NULL, num_threads );

  pthread_t ptids[num_threads];
  for ( int i = 0; i < num_threads; i++ )
    if ( pthread_create ( &ptids[i], NULL, f1, ( void * ) ( uintptr_t ) i ) )
      return 1;

  for ( int i = 0; i < num_threads; i++ )
    pthread_join ( ptids[i], NULL );

  return 0;
}
