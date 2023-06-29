
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>
#include <stdint.h>

#include <generic/rte_atomic.h>

#include "afp_internal.h"

extern uint16_t tot_workers;
extern struct rte_ring *wait_queue;

/* per worker variables */
extern __thread uint16_t worker_id;

/* hardware queue */
extern __thread uint16_t hwq;

extern __thread rte_atomic16_t in_long_request;

extern uint64_t swaps, interruptions, int_no_swaps, invalid_interruptions,
        yields;

#endif
