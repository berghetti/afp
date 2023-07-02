
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
#include "compiler.h"

#define QUANTUM 20  // in us

struct timer
{
  bool active;
  uint64_t deadline;
} __aligned ( CACHE_LINE_SIZE );

static volatile struct timer workers_alarms[MAX_WORKERS];

// static volatile uint64_t workers_alarm[MAX_WORKERS];

static rte_spinlock_t workers_lock[MAX_WORKERS];

static uint32_t tsc_freq_us;
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
  workers_alarms[worker_id].deadline = rte_get_tsc_cycles () + tsc_quantum;
  workers_alarms[worker_id].active = true;
  // if ( !rte_spinlock_trylock ( &workers_lock[worker_id] ) )
  //  return;

  // workers_alarm[worker_id] = rte_get_tsc_cycles () + tsc_quantum;
  // rte_spinlock_unlock ( &workers_lock[worker_id] );
  // INFO ( "%lu: set alarm to worker %u from tryset\n",
  //       rte_get_tsc_cycles (),
  //       worker_id );
}

/* ensure timer is enabled. */
void
timer_set ( uint16_t worker_id )
{
  workers_alarms[worker_id].deadline = rte_get_tsc_cycles () + tsc_quantum;
  workers_alarms[worker_id].active = true;

  // rte_spinlock_lock ( &workers_lock[worker_id] );

  // workers_alarm[worker_id] = rte_get_tsc_cycles () + tsc_quantum;

  // rte_spinlock_unlock ( &workers_lock[worker_id] );
}

/* ensure timer is enabled.
 * permite user set specific delay. */
void
timer_set_delay ( uint16_t worker_id, uint32_t us_delay )
{
  workers_alarms[worker_id].deadline =
          rte_get_tsc_cycles () + us_delay * tsc_freq_us;
  workers_alarms[worker_id].active = true;
  // rte_spinlock_lock ( &workers_lock[worker_id] );

  // workers_alarm[worker_id] = rte_get_tsc_cycles () + us_delay * tsc_freq_us;

  // rte_spinlock_unlock ( &workers_lock[worker_id] );
}

/* ensure timer is disabled */
void
timer_disable ( uint16_t worker_id )
{
  workers_alarms[worker_id].active = false;
  // rte_spinlock_lock ( &workers_lock[worker_id] );

  // workers_alarm[worker_id] = UINT64_MAX;

  // rte_spinlock_unlock ( &workers_lock[worker_id] );
}

void
timer_wait_disabled_state ( uint16_t worker_id )
{
  while ( workers_alarms[worker_id].active )
    cpu_relax ();
}

static void
timer_init ( uint16_t tot_workers )
{
  for ( uint16_t i = 0; i < tot_workers; i++ )
    {
      workers_alarms[i].active = false;
      // rte_spinlock_lock ( &workers_lock[i] );
      // workers_alarms[i] = UINT64_MAX;
      // workers_alarms[i] = UINT64_MAX;
      // rte_spinlock_unlock ( &workers_lock[i] );
    }
}

void
timer_main ( uint16_t tot_workers )
{
  INFO ( "Starting timer management on core %u\n", rte_lcore_id () );

  timer_init ( tot_workers );

  tsc_freq_us = rte_get_tsc_hz () / 1000000U;
  tsc_quantum = QUANTUM * tsc_freq_us;

  INFO ( "TSC frequency: %u ticks per us\n", tsc_freq_us );

  uint64_t deadline, min_deadline, now;

  min_deadline = UINT64_MAX;
  while ( 1 )
    {
      now = rte_get_tsc_cycles ();

      for ( uint16_t i = 0; i < tot_workers; i++ )
        {
          /* worker may be disabling timer */
          // if ( !rte_spinlock_trylock ( &workers_lock[i] ) )
          //  continue;
          if ( !workers_alarms[i].active )
            continue;

          deadline = workers_alarms[i].deadline;

          /* timer disabled */
          // if ( deadline == UINT64_MAX )
          //  // goto next;
          //  continue;

          /* It is not time yet */
          if ( deadline < now )
            {
              if ( deadline < min_deadline )
                min_deadline = deadline;

              // goto next;
              continue;
            }

          // INFO ( "%lu: send interrupt to worker %u\n", deadline, i );

          /* one shot */
          workers_alarms[i].active = false;
          // workers_alarm[i] = UINT64_MAX;
          interrupt_send ( i );

          // next:
          // rte_spinlock_unlock ( &workers_lock[i] );
        }

      /* all timers disabled */
      if ( min_deadline == UINT64_MAX )
        {
          sleep_us ( 5 );
          continue;
        }

      /* same alarm active */
      if ( min_deadline != UINT64_MAX )
        {
          /* wait next shot */
          while ( rte_get_tsc_cycles () < min_deadline )
            cpu_relax ();

          min_deadline = UINT64_MAX;
        }
    }
}
