
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include <cpuid.h>
#include <x86intrin.h>

// return amount cycles by second
uint64_t
get_tsc_freq ( void )
{
#define NS_PER_SEC 1E9
#define CYC_PER_10MHZ 1E7

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

static inline uint32_t
get_cpu_khz ( void )
{
  uint32_t base_mhz, b, c, d;
  __cpuid ( 0x16, base_mhz, b, c, d );

  return base_mhz * 1000;
}

static uint64_t
tsc_hz_by_cpuid ()
{
  uint32_t eax, ebx, ecx, edx;
  __cpuid ( 0x15, eax, ebx, ecx, edx );

  printf ( "%x %d %d %d\n", eax, ebx, ecx, edx );
  if ( !eax || !ebx )
    return 0;

  if ( !ecx )
    ecx = get_cpu_khz () * eax / ebx;

  uint64_t tsc_hz = ecx * ebx / eax;

  return tsc_hz;
}

int
main ( void )
{

  uint32_t max_level;

  max_level = __get_cpuid_max ( 0, NULL );

  printf ( "%d\n", max_level );

  printf ( "%lu\n", tsc_hz_by_cpuid () );
  printf ( "%lu\n", get_tsc_freq () );

  return 0;
}
