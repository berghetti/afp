

#include <stdint.h>
#include <stdbool.h>

#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_atomic.h>
#include <rte_spinlock.h>

#include "interrupt.h"
#include "debug.h"
#include "afp_internal.h"
#include "compiler.h"

#define QUANTUM 20  // in us

static rte_spinlock_t workers_lock[MAX_WORKERS] __aligned ( CACHE_LINE_SIZE );

static uint64_t workers_alarms[MAX_WORKERS] __aligned ( CACHE_LINE_SIZE );

static uint64_t tsc_freq_us;
static uint64_t tsc_quantum;

static inline void
cpu_relax ( void )
{
  asm volatile( "pause" );
}

void
sleep_us ( uint32_t us )
{
  uint64_t wait = rte_get_tsc_cycles () + us * tsc_freq_us;

  while ( rte_get_tsc_cycles () < wait )
    cpu_relax ();
}

/* if not taken lock, this was called from interrupt handler
 * and main line is in timer_disable, or 'timer_main' core is sending
 * a interrupt to we. */
void
timer_tryset ( uint16_t worker_id )
{
  if ( !rte_spinlock_trylock ( &workers_lock[worker_id] ) )
    return;

  workers_alarms[worker_id] = rte_get_tsc_cycles () + tsc_quantum;
  rte_spinlock_unlock ( &workers_lock[worker_id] );

  DEBUG ( "set alarm to worker %u from tryset\n", worker_id );
}

/* ensure timer is enabled. */
void
timer_set ( uint16_t worker_id )
{
  rte_spinlock_lock ( &workers_lock[worker_id] );

  workers_alarms[worker_id] = rte_get_tsc_cycles () + tsc_quantum;

  rte_spinlock_unlock ( &workers_lock[worker_id] );

  DEBUG ( "set alarm to worker %u\n", worker_id );
}

/* ensure timer is enabled.
 * permite user set specific delay. */
void
timer_set_delay ( uint16_t worker_id, uint32_t us_delay )
{
  rte_spinlock_lock ( &workers_lock[worker_id] );

  workers_alarms[worker_id] = rte_get_tsc_cycles () + us_delay * tsc_freq_us;

  rte_spinlock_unlock ( &workers_lock[worker_id] );
}

/* ensure timer is disabled */
void
timer_disable ( uint16_t worker_id )
{
  rte_spinlock_lock ( &workers_lock[worker_id] );

  workers_alarms[worker_id] = UINT64_MAX;

  rte_spinlock_unlock ( &workers_lock[worker_id] );
}

static void
timer_init ( uint16_t tot_workers )
{
  for ( uint16_t i = 0; i < tot_workers; i++ )
    {
      rte_spinlock_lock ( &workers_lock[i] );
      workers_alarms[i] = UINT64_MAX;
      rte_spinlock_unlock ( &workers_lock[i] );
    }
}

void
timer_main ( uint16_t tot_workers )
{
  INFO ( "Starting timer management on core %u\n", rte_lcore_id () );

  timer_init ( tot_workers );

  tsc_freq_us = rte_get_tsc_hz () / 1000000U;
  tsc_quantum = QUANTUM * tsc_freq_us;

  INFO ( "TSC frequency: %lu ticks per us\n", tsc_freq_us );
  INFO ( "QUANTUM: %u us\n", QUANTUM );

  uint64_t deadline, min_deadline, now;

  min_deadline = UINT64_MAX;
  while ( 1 )
    {
      now = rte_get_tsc_cycles ();

      for ( uint16_t i = 0; i < tot_workers; i++ )
        {
          /* worker may be disabling timer */
          if ( !rte_spinlock_trylock ( &workers_lock[i] ) )
            continue;

          deadline = workers_alarms[i];

          /* timer disabled */
          if ( deadline == UINT64_MAX )
            goto next;

          /* It is not time yet */
          if ( now < deadline )
            {
              if ( deadline < min_deadline )
                min_deadline = deadline;

              goto next;
            }

          DEBUG ( "send interrupt to worker %u\n", i );

          /* one shot */
          workers_alarms[i] = UINT64_MAX;
          interrupt_send ( i );

        next:
          rte_spinlock_unlock ( &workers_lock[i] );
        }

      /* all timers disabled */
      if ( min_deadline == UINT64_MAX )
        continue;

      /* some alarm active, wait next shot */
      while ( rte_get_tsc_cycles () < min_deadline )
        cpu_relax ();

      min_deadline = UINT64_MAX;
    }
}
