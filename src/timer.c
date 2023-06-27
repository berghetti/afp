
#include <generic/rte_cycles.h>
#include <stdint.h>
#include <stdbool.h>

#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_atomic.h>
#include <rte_spinlock.h>

#include "interrupt.h"
#include "debug.h"
#include "afp_internal.h"

#define QUANTUM 20  // in us

static uint64_t workers_alarm[MAX_WORKERS];

static rte_spinlock_t workers_lock[MAX_WORKERS];

static inline void
cpu_relax ( void )
{
  asm volatile( "pause" );
}

/* if not taken lock, this was called from interrupt handler
 * and main line is in timer_disable, or 'timer_main' core is sending
 * a interrupt to we.*/
void
timer_set ( uint16_t worker_id )
{
  // rte_atomic64_set ( &workers_alarm[worker_id], rte_get_tsc_cycles () );
  if ( !rte_spinlock_trylock ( &workers_lock[worker_id] ) )
    return;

  workers_alarm[worker_id] = rte_get_tsc_cycles ();
  rte_spinlock_unlock ( &workers_lock[worker_id] );
}

/* ensure timer is disabled */
void
timer_disable ( uint16_t worker_id )
{
  rte_spinlock_lock ( &workers_lock[worker_id] );

  workers_alarm[worker_id] = UINT64_MAX;

  rte_spinlock_unlock ( &workers_lock[worker_id] );
}

static void
timer_init ( uint16_t tot_workers )
{
  for ( uint16_t i = 0; i < tot_workers; i++ )
    {
      workers_alarm[i] = UINT64_MAX;
    }
}

void
timer_main ( uint16_t tot_workers )
{
  INFO ( "Starting timer management on core %u\n", rte_lcore_id () );

  timer_init ( tot_workers );

  uint64_t cycles_per_us = rte_get_tsc_hz () / 1000000UL;
  INFO ( "TSC frequency: %lu ticks per us\n", cycles_per_us );

  uint64_t quantum = QUANTUM * cycles_per_us;

  uint64_t min;
  uint16_t worker;
  while ( 1 )
    {
      min = UINT64_MAX;

      // find first worker started alarm
      for ( uint16_t i = 0; i < tot_workers; i++ )
        {
          uint64_t value = workers_alarm[i];
          if ( value < min )
            {
              min = value;
              worker = i;
            }
        }

      if ( min == UINT64_MAX )
        continue;

      min += quantum;

      while ( rte_get_tsc_cycles () < min )
        cpu_relax ();

      // worker not disarm alarm
      if ( !rte_spinlock_trylock ( &workers_lock[worker] ) )
        continue;

      if ( workers_alarm[worker] != UINT64_MAX )
        {
          DEBUG ( "Send interrupt to worker %u\n", worker );
          workers_alarm[worker] = UINT64_MAX;
          interrupt_send ( worker );
        }

      rte_spinlock_unlock ( &workers_lock[worker] );
    }
}
