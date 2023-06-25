

#include <generic/rte_cycles.h>
#include <stdint.h>

#include "afp.h"
#include "timer.h"
#include "debug.h"

void
afp_send_feedback ( afp_ctx_t *ctx, enum feedback feedback )
{
  uint64_t now = rte_get_tsc_cycles ();
  uint16_t worker_id = ctx->worker_id;

  switch ( feedback )
    {
      case START_LONG:
        DEBUG ( "%lu: Starting timer on worker %u\n", now, worker_id );
        ctx->in_long_request = true;
        timer_set ( worker_id, now );
        break;

      case FINISHED_LONG:
        DEBUG ( "Worker %u finished long request\n", worker_id );
        ctx->in_long_request = false;
        timer_disable ( worker_id );
    }
}
