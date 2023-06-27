

#include <generic/rte_cycles.h>
#include <stdint.h>

#include "afp.h"
#include "timer.h"
#include "debug.h"

void
afp_send_feedback ( afp_ctx_t *ctx, enum feedback feedback )
{
  uint16_t worker_id = ctx->worker_id;

  switch ( feedback )
    {
      case START_LONG:
        ctx->in_long_request = true;
        timer_set ( worker_id );
        DEBUG ( "Starting timer on worker %u\n", worker_id );
        break;

      case FINISHED_LONG:
        ctx->in_long_request = false;
        timer_disable ( worker_id );
        DEBUG ( "Worker %u finished long request\n", worker_id );
    }
}
