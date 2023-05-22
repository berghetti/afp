#ifndef AFP_H
#define AFP_H

#include "afp_internal.h"

enum feedback
{
  START_LONG = 0,
  FINISHED_LONG
};

void
afp_send_feedback ( afp_ctx_t *ctx, enum feedback f );

#endif
