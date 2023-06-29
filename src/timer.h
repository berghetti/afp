
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* may not activate the timer, see timer.c */
void
timer_tryset ( uint16_t worker_id );

/* functions that ensure timer is enabled/disables */

void
timer_set ( uint16_t worker_id );

void
timer_set_delay ( uint16_t worker_id, uint32_t delay );

void
timer_disable ( uint16_t worker_id );

/* main function to 'timer core'*/
void
timer_main ( uint16_t tot_workers );

#endif
