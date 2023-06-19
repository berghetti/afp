
// common utils for tests (benchmark)

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <time.h>
#include <x86intrin.h>

#define NS_PER_SEC 1E9

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

static void
pin_to_cpu ( int cpu )
{
  cpu_set_t cpus;
  CPU_ZERO ( &cpus );
  CPU_SET ( cpu, &cpus );
  if ( pthread_setaffinity_np ( pthread_self (), sizeof ( cpus ), &cpus ) )
    {
      printf ( "Could not pin thread to core %d.\n", cpu );
      exit ( 1 );
    }
}

static uint32_t cycles_by_us;

static inline double
cycles2us ( uint32_t cycles )
{
  return ( double ) cycles / cycles_by_us;
}

static int
cmp_uint32 ( const void *restrict a, const void *restrict b )
{
  uint32_t v1, v2;

  v1 = *( uint32_t * ) a;
  v2 = *( uint32_t * ) b;

  return ( v1 > v2 ) - ( v1 < v2 );
}

static inline uint32_t
percentile ( uint32_t *buff, size_t len, float percentile )
{
  unsigned int idx = len * percentile;
  return buff[idx];
}

static void
print ( uint32_t *buff, size_t size, char *msg, uint32_t discard )
{
  // ignore firsts 'discard' values
  buff += discard;
  size -= discard;

  qsort ( buff, size, sizeof ( *buff ), cmp_uint32 );

  uint32_t min = 0, mean, p99, p999, max;
  min = buff[0];
  mean = percentile ( buff, size, 0.50f );
  p99 = percentile ( buff, size, 0.99f );
  p999 = percentile ( buff, size, 0.999f );
  max = buff[size - 1];

  printf ( "\n%s\n"
           "  Min:   %u (%.2f us)\n"
           "  50%%:   %u (%.2f us)\n"
           "  99%%:   %u (%.2f us)\n"
           "  99.9%%: %u (%.2f us)\n"
           "  Max:   %u (%.2f us)\n",
           msg,
           min,
           cycles2us ( min ),
           mean,
           cycles2us ( mean ),
           p99,
           cycles2us ( p99 ),
           p999,
           cycles2us ( p999 ),
           max,
           cycles2us ( max ) );
}

static void
init_util ( void )
{
  cycles_by_us = get_tsc_freq () / 1000000UL;
}

#endif
