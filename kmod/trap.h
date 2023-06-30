
#ifndef TRAP_H
#define TRAP_H

typedef void ( *user_handler ) ( void );

/* set high level user handler interruption in trap.c */
int
trap_init ( user_handler handler );

/* send interrupt to 'core' */
void
trap_send_interrupt ( int core );

void
trap_free ( void );

#endif
