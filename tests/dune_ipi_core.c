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
static uint32_t cycles_by_us;
static uint32_t samples[RUNS];
static uint32_t tsc_worker[RUNS], tsc_sender_worker[RUNS];
static uint64_t tsc_starts[RUNS], tsc_ends[RUNS];
static uint64_t volatile sender_start_tsc, worker_start_tsc;

static volatile int wait_handler = 1;

static volatile uint32_t i_handler = 0;

static void
jmp_for_me ( struct dune_tf *tf )
{
  uint64_t now = __rdtsc ();
  tsc_sender_worker[i_handler] = now - sender_start_tsc;
  tsc_worker[i_handler] = now - worker_start_tsc;

  i_handler++;

  dune_apic_eoi();
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
  if ( pthread_setaffinity_np ( pthread_self (), sizeof ( cpus ), &cpus ) )
    {
      printf ( "Could not pin thread to core %d.\n", WORKER_CORE );
      exit ( 1 );
    }

  volatile int ret = dune_enter();
  if (ret)
    {
      fprintf(stderr, "failed to enter dune in thread 2\n");
      exit(1);
    }

  printf ( "Started worker thread on core %d\n", sched_getcpu() );

  dune_apic_init_rt_entry();
  dune_register_intr_handler(TEST_VECTOR, jmp_for_me);
  asm volatile("mfence" ::: "memory");

  worker_start_tsc = __rdtsc ();
  worker_ready = 1;
  while ( !worker_stop )
    worker_start_tsc = __rdtsc ();

  return NULL;
}

static void
sender ( void )
{
  printf ( "Started sender thread on core %u\n", sched_getcpu() );

  uint64_t end, start = __rdtsc ();
  for ( unsigned int i = 0; i < RUNS; i++ )
    {
      tsc_starts[i] = __rdtsc ();
      sender_start_tsc = tsc_starts[i];

      dune_apic_send_ipi(TEST_VECTOR, dune_apic_id_for_cpu(WORKER_CORE, NULL));

      tsc_ends[i] = __rdtsc ();

      while ( wait_handler )
        asm volatile( "pause" );

      wait_handler = 1;
    }
  end = __rdtsc ();

  uint64_t avg = ( end - start ) / RUNS;
  printf ( "Sender average: %lu (%.2f us)", avg, cycles2us ( avg ) );

  worker_stop = 1;

  for ( unsigned int i = 0; i < RUNS; i++ )
    samples[i] = tsc_ends[i] - tsc_starts[i];

  print ( samples, RUNS, "Sender" );
  print ( tsc_sender_worker, i_handler, "Sender/Worker" );
  print ( tsc_worker, i_handler, "Worker" );

  printf ( "\nDUNE IPIs handled: %d\n", i_handler );
}

int
main ( int argc, char **argv )
{
  cycles_by_us = get_tsc_freq () / 1000000;

  printf ( "Samples: %lu\n"
           "Cycles by us: %u\n",
           RUNS,
           cycles_by_us );
  
  cpu_set_t cpus;
  CPU_ZERO ( &cpus );
  CPU_SET ( SENDER_CORE, &cpus );
  if ( pthread_setaffinity_np ( pthread_self (), sizeof ( cpu_set_t ), &cpus ) )
    {
      printf ( "Could not pin thread to core %d.\n", SENDER_CORE );
      exit ( 1 );
    }
   
  volatile int ret = dune_init_and_enter();
  if (ret)
    {
       fprintf(stderr, "failed to initialize dune\n");
       return ret;
    }
  dune_apic_init_rt_entry();

  pthread_t tid;
  pthread_create ( &tid, NULL, worker, NULL );

  while ( !worker_ready )
    ;

  sender ();

  pthread_join ( tid, NULL );

  return 0;
}

