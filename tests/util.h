
#ifndef UTIL_H
#define UTIL_H

static int
cmp_uint32 ( const void *a, const void *b )
{
  return *( uint32_t * ) a - *( uint32_t * ) b;
}

static inline uint32_t
percentile ( uint32_t *buff, size_t len, float percentile )
{
  unsigned int idx = ( len * percentile );
  return buff[idx];
}

#endif
