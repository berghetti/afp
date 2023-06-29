
#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

void
interrupt_send ( uint16_t worker_id );

void
interrupt_register_work_tid ( uint16_t worker, pid_t tid );

void
interrupt_register_worker ( uint16_t worker_id, int core );

void
interrupt_init ( void ( *handler ) ( int ) );

#endif
