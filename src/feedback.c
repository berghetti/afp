
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
        in_long_request = true;
        timer_set ( worker_id );
        DEBUG ( "Worker %u started long request\n", worker_id );
        break;

      case FINISHED_LONG:
        // asm volatile( "cli" );
        in_long_request = false;
        timer_disable ( worker_id );
        DEBUG ( "Worker %u finished long request\n", worker_id );
    }
}
