
#include <generic/rte_cycles.h>
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
        rte_atomic16_set ( &in_long_request, 1 );
        // INFO ( "%lu: worker %u starting timer\n",
        //       rte_get_tsc_cycles (),
        //       worker_id );
        timer_set ( worker_id );
        // INFO ( "%lu: worker %u finished timer set\n",
        //       rte_get_tsc_cycles (),
        //       worker_id );
        DEBUG ( "Worker %u started long request\n", worker_id );
        break;

      case FINISHED_LONG:
        rte_atomic16_set ( &in_long_request, 0 );
        timer_disable ( worker_id );
        DEBUG ( "Worker %u finished long request\n", worker_id );
    }
}
