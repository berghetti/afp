
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

void
worker ( int fd )
{
  pin_to_cpu ( WORKER_CORE );
  printf ( "Started worker thread on core %u\n", sched_getcpu () );

  worker_start_tsc = __rdtsc ();
  ioctl ( fd, KMOD_START_TIMER, _trap_entry );

  while ( i_handler < RUNS )
    worker_start_tsc = __rdtsc ();

  ioctl ( fd, KMOD_STOP_TIMER );

  print ( samples, RUNS, "Worker overhead", 0 );
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

  int fd = open ( KMOD_HRTIMER_PATH, O_RDWR );
  if ( fd < 0 )
    {
      perror ( "Error to open character " KMOD_HRTIMER_PATH );
      return 1;
    }

  worker ( fd );

  close ( fd );

  printf ( "\nTotal interruptions handled %u\n", i_handler );
  return 0;
}
