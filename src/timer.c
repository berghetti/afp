
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

static uint64_t tsc_freq_us;

static uint64_t tsc_quantum;

static inline void
cpu_relax ( void )
{
  asm volatile( "pause" );
}

/* if not taken lock, this was called from interrupt handler
 * and main line is in timer_disable, or 'timer_main' core is sending
 * a interrupt to we. */
void
timer_set ( uint16_t worker_id )
{
  if ( !rte_spinlock_trylock ( &workers_lock[worker_id] ) )
    return;

  workers_alarm[worker_id] = rte_get_tsc_cycles ();
  rte_spinlock_unlock ( &workers_lock[worker_id] );
}

void
timer_set_delay ( uint16_t worker_id, uint32_t us_delay )
{
  if ( !rte_spinlock_trylock ( &workers_lock[worker_id] ) )
    return;

  workers_alarm[worker_id] = rte_get_tsc_cycles () + us_delay * tsc_freq_us;
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
      rte_spinlock_lock ( &workers_lock[i] );
      workers_alarm[i] = UINT64_MAX;
      rte_spinlock_unlock ( &workers_lock[i] );
    }
}

void
timer_main ( uint16_t tot_workers )
{
  INFO ( "Starting timer management on core %u\n", rte_lcore_id () );

  timer_init ( tot_workers );

  tsc_freq_us = rte_get_tsc_hz () / 1000000UL;
  tsc_quantum = QUANTUM * tsc_freq_us;

  INFO ( "TSC frequency: %lu ticks per us\n", tsc_freq_us );

  uint64_t min;
  uint16_t worker, i;
  while ( 1 )
    {
      // find first worker started alarm
      min = UINT64_MAX;
      for ( i = 0; i < tot_workers; i++ )
        {
          uint64_t value = workers_alarm[i];
          if ( value < min )
            {
              min = value;
              worker = i;
            }
        }

      if ( UINT64_MAX == min )
        continue;

      min += tsc_quantum;

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
