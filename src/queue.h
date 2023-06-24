#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include <generic/rte_spinlock.h>
#include <rte_branch_prediction.h>

#include "compiler.h"
#include "afp_internal.h"

/*
 * Ring queue FIFO.
 * Usable size is QUEUE_SIZE - 1.
 */

#define MASK ( QUEUE_SIZE - 1 )

struct queue
{
  rte_spinlock_t lock;
  uint32_t head;
  uint32_t tail;
  void *data[QUEUE_SIZE];
} __aligned ( CACHE_LINE_SIZE );

static inline void
queue_init ( struct queue *q )
{
  rte_spinlock_init ( &q->lock );
  q->head = q->tail = 0;
}

static inline void
queue_lock ( struct queue *q )
{
  rte_spinlock_lock ( &q->lock );
}

static inline int
queue_trylock ( struct queue *q )
{
  return rte_spinlock_trylock ( &q->lock );
}

static inline void
queue_unlock ( struct queue *q )
{
  rte_spinlock_unlock ( &q->lock );
}

static inline int
queue_is_empty ( struct queue *q )
{
  return q->head == q->tail;
}

static inline int
queue_is_full ( struct queue *q )
{
  return ( ( q->head + 1 ) & MASK ) == q->tail;
}

// count elements used
static inline int
queue_count ( struct queue *q )
{
  return ( QUEUE_SIZE + q->head - q->tail ) & MASK;
}

static inline unsigned int
queue_count_free ( struct queue *q )
{
  return QUEUE_SIZE - 1 - queue_count ( q );
}

static inline void *
queue_enqueue ( struct queue *restrict q, void *data )
{
  if ( unlikely ( queue_is_full ( q ) ) )
    return NULL;

  q->data[q->head++ & MASK] = data;
  return data;
}

static inline uint32_t
queue_enqueue_bulk ( struct queue *restrict q, void **buff, uint32_t size )
{
  if ( unlikely ( queue_count_free ( q ) < size ) )
    return 0;

  for ( uint32_t i = 0; i < size; i++ )
    q->data[q->head++ & MASK] = buff[i];

  return size;
}

static inline void *
queue_dequeue ( struct queue *q )
{
  if ( queue_is_empty ( q ) )
    return NULL;

  return q->data[q->tail++ & MASK];
}

static inline void
queue_stealing ( struct queue *restrict dst,
                 struct queue *restrict src,
                 uint32_t size )
{
  while ( size-- )
    dst->data[dst->head++ & MASK] = src->data[src->tail++ & MASK];
}

#endif
