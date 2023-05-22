
#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

void
interrupt_send ( uint16_t worker_id );

void
interrupt_register_work_tid ( uint16_t worker, pid_t tid );

void
interrupt_enable ( void );

void
interrupt_disable ( void );

void
interrupt_init ( void ( *handler ) ( int ) );

#endif
