
#ifndef UTIL_H
#define UTIL_H

#include <time.h>
#include <x86intrin.h>

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

#define NS_PER_SEC 1E9
#define CYC_PER_10MHZ 1E7

// return amount cycles by second
uint64_t
get_tsc_freq ( void )
{

  struct timespec sleeptime = { .tv_nsec = NS_PER_SEC / 2 }; /* 1/2 second */
  struct timespec t_start, t_end;

  uint64_t tsc_hz;

  if ( clock_gettime ( CLOCK_MONOTONIC_RAW, &t_start ) == 0 )
    {
      uint64_t ns, end, start = __rdtsc ();
      nanosleep ( &sleeptime, NULL );
      clock_gettime ( CLOCK_MONOTONIC_RAW, &t_end );
      end = __rdtsc ();
      ns = ( ( t_end.tv_sec - t_start.tv_sec ) * NS_PER_SEC );
      ns += ( t_end.tv_nsec - t_start.tv_nsec );

      double secs = ( double ) ns / NS_PER_SEC;
      tsc_hz = ( uint64_t ) ( ( end - start ) / secs );

      return tsc_hz;
    }

  return 0;
}

#endif
