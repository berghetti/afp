
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* may not activate the timer, see timer.c*/
void
timer_set ( uint16_t worker_id );

/* only return if timer is disabled */
void
timer_disable ( uint16_t worker_id );

/* main function to 'timer core'*/
void
timer_main ( uint16_t tot_workers );

#endif
