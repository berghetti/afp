
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
#ifndef NDEBUG
        uint64_t start = rte_get_tsc_cycles ();
#endif
        timer_set ( worker_id );
        DEBUG ( "worker %u overhead set timer: %u\n",
                worker_id,
                rte_get_tsc_cycles () - start );
        // DEBUG ( "Worker %u started long request\n", worker_id );
        break;

      case FINISHED_LONG:
        rte_atomic16_set ( &in_long_request, 0 );
        timer_disable ( worker_id );
        DEBUG ( "Worker %u finished long request\n", worker_id );
    }
}
