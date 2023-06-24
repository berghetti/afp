
#include <stdint.h>

#include <rte_lcore.h>
#include <rte_cycles.h>

#include "interrupt.h"
#include "debug.h"
#include "afp_internal.h"

#define QUANTUM 5  // in us

static uint64_t workers_alarm[MAX_WORKERS];

static inline void
cpu_relax ( void )
{
  asm volatile( "pause" );
}

void
timer_set ( uint16_t worker_id, uint64_t now )
{
  workers_alarm[worker_id] = now;
}

void
timer_disable ( uint16_t worker_id )
{
  workers_alarm[worker_id] = UINT64_MAX;
}

void
timer_main ( uint16_t tot_workers )
{

  INFO ( "Starting timer management on core %u\n", rte_lcore_id () );

  uint64_t cycles_per_us = rte_get_tsc_hz () / 1000000UL;
  INFO ( "Cycles per us: %lu\n", cycles_per_us );

  uint64_t quantum = QUANTUM * cycles_per_us;

  uint64_t min, wait;
  uint16_t worker;
  while ( 1 )
    {
      min = UINT64_MAX;

      // find first worker started alarm
      for ( uint16_t i = 0; i < tot_workers; i++ )
        {
          if ( workers_alarm[i] < min )
            {
              min = workers_alarm[i];
              worker = i;
            }
        }

      if ( min == UINT64_MAX )
        continue;

      wait = quantum + min;

      while ( rte_get_tsc_cycles () < wait )
        cpu_relax ();

      // worker not disarm alarm?
      if ( workers_alarm[worker] != UINT64_MAX )
        {
          workers_alarm[worker] = UINT64_MAX;

          // DEBUG ( "%lu: Send interrupt to worker %u\n", last_sent, worker );
          interrupt_send ( worker );
        }
    }
}

void
timer_init ( uint16_t tot_workers )
{
  for ( uint16_t i = 0; i < tot_workers; i++ )
    workers_alarm[i] = UINT64_MAX;
}
