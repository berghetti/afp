
#include <stdbool.h>
#include <stdint.h>

#include "afp_internal.h"

uint16_t tot_workers;
struct rte_ring *wait_queue;

__thread uint16_t worker_id;

/* hardware queue */
__thread uint16_t hwq;

__thread bool in_long_request;

/* statistics */
uint64_t swaps, interruptions, int_no_swaps, invalid_interruptions, yields;
