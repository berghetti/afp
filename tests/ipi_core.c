
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
#include "kmod_ipi.h"

#define SENDER_CORE 0
#define WORKER_CORE 1

#define RUNS 100000UL

static volatile int worker_ready, worker_stop;
static uint32_t cycles_by_us;
static uint32_t samples[RUNS];
static uint32_t tsc_worker[RUNS], tsc_sender_worker[RUNS];
static uint64_t tsc_starts[RUNS], tsc_ends[RUNS];
static uint64_t volatile sender_start_tsc, worker_start_tsc;

static volatile int wait_handler = 1;

static volatile uint32_t i_handler = 0;

void
jmp_for_me ( void )
{
  // usleep ( 5 );
  uint64_t now = __rdtsc ();
  tsc_sender_worker[i_handler] = now - sender_start_tsc;
  tsc_worker[i_handler] = now - worker_start_tsc;

  i_handler++;

  wait_handler = 0;
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

void *
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

  worker_ready = 1;
  while ( !worker_stop )
    worker_start_tsc = __rdtsc ();

  return NULL;
}

static void
sender ( int fd )
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

  struct req_ipi req = { .core = 1, ._trap_entry = _trap_entry };

  uint64_t end, start = __rdtsc ();
  for ( unsigned int i = 0; i < RUNS; i++ )
    {
      tsc_starts[i] = __rdtsc ();
      sender_start_tsc = tsc_starts[i];

      ioctl ( fd, KMOD_IPI_SEND, &req );

      tsc_ends[i] = __rdtsc ();

      while ( wait_handler )
        asm volatile( "pause" );

      wait_handler = 1;
    }
  end = __rdtsc ();

  uint64_t avg = ( end - start ) / RUNS;
  printf ( "Average: %lu (%.2f us)", avg, cycles2us ( avg ) );

  worker_stop = 1;

  for ( unsigned int i = 0; i < RUNS; i++ )
    samples[i] = tsc_ends[i] - tsc_starts[i];

  print ( samples, RUNS, "Sender" );
  print ( tsc_sender_worker, i_handler, "Sender/Worker" );
  print ( tsc_worker, i_handler, "Worker" );

  printf ( "\nIPIs handled: %d\n", i_handler );
}

int
main ( int argc, char **argv )
{
  cycles_by_us = get_tsc_freq () / 1000000;

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

  pthread_t tid;
  pthread_create ( &tid, NULL, worker, NULL );

  while ( !worker_ready )
    ;

  sender ( fd );
  pthread_join ( tid, NULL );

  close ( fd );
  return 0;
}
