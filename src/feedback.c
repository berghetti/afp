
#include <stdint.h>

#include "globals.h"
#include "feedback.h"
#include "timer.h"
#include "debug.h"

void
afp_send_feedback ( enum feedback f )
{
  switch ( f )
    {
      case START_LONG:
        // asm volatile( "sti" );
        rte_atomic16_set ( &in_long_request, 1 );
        timer_set ( worker_id );
        DEBUG ( "Worker %u started long request\n", worker_id );
        break;

      case FINISHED_LONG:
        // asm volatile( "cli" );
        rte_atomic16_set ( &in_long_request, 0 );
        timer_disable ( worker_id );
        DEBUG ( "Worker %u finished long request\n", worker_id );
    }
}
