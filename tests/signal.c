
/* Single test overhead to send signal and get signal handler using various
 * methods to send signal
 *
 * taskset -c 1 ./signal
 */

#define _GNU_SOURCE
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/syscall.h>

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#define gettid() syscall ( SYS_gettid )
#define tgkill( pid, tid, sig ) syscall ( SYS_tgkill, pid, tid, sig )
#endif

#include <x86intrin.h>

#define RUNS 100000UL

static volatile uint64_t ts;

static uint64_t i;
static uint32_t samples[RUNS];

static void
handler ( int sig )
{
  uint32_t t = __rdtsc () - ts;

  samples[i++] = t;
}

static int
cmp ( const void *a, const void *b )
{
  return *( uint32_t * ) a - *( uint32_t * ) b;
}

static uint32_t
percentile ( uint32_t *buff, size_t len, float percentile )
{
  unsigned int idx = ( len * percentile );
  return buff[idx];
}

static inline void
reset ()
{
  i = 0;
}

static void
print ( char *msg )
{
  qsort ( samples, RUNS, sizeof ( samples[0] ), cmp );

  uint64_t i = RUNS;
  uint64_t sum = 0;
  while ( i-- )
    sum += samples[i];

  printf ( "\n%s\n"
           "  Min: %u\n"
           "  Max: %u\n"
           "  Avg: %lu\n"
           "  Median: %u\n"
           "  99%%: %u\n"
           "  99.9%%: %u\n",
           msg,
           samples[0],
           samples[RUNS - 1],
           sum / RUNS,
           percentile ( samples, RUNS, 0.50f ),
           percentile ( samples, RUNS, 0.99f ),
           percentile ( samples, RUNS, 0.999f ) );

  reset ();
}

static void
f1 ( void )
{
  uint64_t i = RUNS;
  while ( i-- )
    {
      ts = __rdtsc ();
      raise ( SIGUSR1 );
    }

  print ( "Using raise" );
}

static void
f2 ( void )
{

  pid_t pid = getpid ();
  uint64_t i = RUNS;
  while ( i-- )
    {
      ts = __rdtsc ();
      kill ( pid, SIGUSR1 );
    }

  print ( "Using kill" );
}

static void
f3 ( void )
{
  union sigval sv = { .sival_int = 0 };
  pid_t pid = getpid ();
  uint64_t i = RUNS;
  while ( i-- )
    {
      ts = __rdtsc ();
      sigqueue ( pid, SIGUSR1, sv );
    }

  print ( "Using sigqueue" );
}

static void
f4 ( void )
{
  pid_t tgid, tid;
  tgid = tid = gettid ();  // single thread
  uint64_t i = RUNS;
  while ( i-- )
    {
      ts = __rdtsc ();
      tgkill ( tgid, tid, SIGUSR1 );
    }

  print ( "Using tgkill" );
}

static void
f5 ( void )
{
  pid_t tgid, tid;
  tgid = tid = gettid ();
  uint64_t i = RUNS;
  while ( i-- )
    {
      ts = __rdtsc ();
      syscall ( SYS_tgkill, tgid, tid, SIGUSR1 );
    }

  print ( "Using SYSCALL(tgkill)" );
}

int
main ( int argc, char **argv )
{
  struct sigaction act = {
    .sa_handler = handler,
  };

  sigaction ( SIGUSR1, &act, NULL );

  printf ( "Runs %lu\n", RUNS );

  f1 ();
  f2 ();
  f3 ();
  f4 ();
  f5 ();

  return 0;
}
