
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void
timer_set ( uint16_t worker_id, uint64_t now );

void
timer_disable ( uint16_t worker_id );

void
timer_init ( uint16_t tot_workers );

void
timer_main ( uint16_t tot_workers );

#endif
