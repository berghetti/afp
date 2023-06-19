
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
#include "kmod_hrtimer.h"

#define WORKER_CORE 7

#define RUNS 100000UL

#define DISCARD 10  // ignore first samples

static uint32_t cycles_by_us;
static uint32_t samples[RUNS], timer_frequency[RUNS];
static uint64_t volatile worker_start_tsc;

static volatile uint32_t i_handler = 0;

void
jmp_for_me ( void )
{
  static uint64_t last_now = 0;

  uint64_t now = __rdtsc ();

  samples[i_handler] = now - worker_start_tsc;

  timer_frequency[i_handler] = now - last_now;

  last_now = now;

  i_handler++;
}

static inline double
cycles2us ( uint32_t cycles )
{
  return ( double ) cycles / cycles_by_us;
}

static void
print ( uint32_t *buff, size_t size, char *msg )
{
  buff += DISCARD;
  size -= DISCARD;

  qsort ( buff, size, sizeof ( uint32_t ), cmp_uint32 );

  uint32_t min = 0, mean, p99, p999, max;
  min = buff[0];
  mean = percentile ( buff, size, 0.50f );
  p99 = percentile ( buff, size, 0.99f );
  p999 = percentile ( buff, size, 0.999f );
  max = buff[size - 1];

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

void
worker ( int fd )
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

  worker_start_tsc = __rdtsc ();
  ioctl ( fd, KMOD_START_TIMER, _trap_entry );

  while ( i_handler < RUNS )
    worker_start_tsc = __rdtsc ();

  ioctl ( fd, KMOD_STOP_TIMER );

  sleep ( 1 );

  print ( samples, RUNS, "Worker overhead" );
  print ( timer_frequency, RUNS, "Timer frequency" );
}

int
main ( int argc, char **argv )
{
  cycles_by_us = get_tsc_freq () / 1000000;

  printf ( "Samples: %lu\n"
           "Cycles by us: %u\n",
           RUNS,
           cycles_by_us );

  int fd = open ( KMOD_HRTIMER_PATH, O_RDWR );
  if ( fd < 0 )
    {
      perror ( "Error to open character " KMOD_HRTIMER_PATH );
      return 1;
    }

  worker ( fd );

  close ( fd );
  printf ( "Total interruptions %u\n", i_handler );
  return 0;
}
