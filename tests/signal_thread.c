
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

static int num_threads;
static int us;

static pthread_barrier_t barrier;

static __thread volatile uint64_t tsc_start;
static __thread uint32_t samples[RUNS];

static void
handler ( int sig )
{
  static __thread uint64_t i = 0;
  samples[i++] = __rdtsc () - tsc_start;
}

static void *
f1 ( void *arg )
{
  int i = ( int ) ( uintptr_t ) arg;

  pin_to_cpu ( i );

  pid_t tgid = getpid ();
  pid_t tid = gettid ();

  uint64_t end;

  uint64_t count = RUNS;
  // First execution not await others threads. Assume as warm up.
  while ( count-- )
    {
      tsc_start = __rdtsc ();
      tgkill ( tgid, tid, SIGUSR1 );

      // ensure specific misalign between threads start
      pthread_barrier_wait ( &barrier );

      end = __rdtsc () + us * i * cycles_by_us;
      while ( __rdtsc () < end )
        ;
    }

  char buff[14];
  snprintf ( buff, sizeof ( buff ), "Thread %u", i );
  print ( samples, RUNS, buff, 0 );

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

  init_util ();

  num_threads = atoi ( argv[1] );
  num_threads += ( num_threads == 0 );

  us = atoi ( argv[2] );

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
