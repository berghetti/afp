
// Single test overhead between send signal and get signal handler

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "cycles_count.h"

#define RUNS 100000UL

extern pid_t gettid (void);
extern void tgkill (pid_t, pid_t, int);

static volatile uint64_t ts;

static uint64_t i;
static uint32_t samples[RUNS];

static void
handler (int sig, siginfo_t *info, void *ucontext)
{
  uint32_t t = rdtsc_end () - ts;

  samples[i++] = t;
}

static int
cmp (const void *a, const void *b)
{
  return *(uint32_t *)a - *(uint32_t *)b;
}

static uint32_t
percentile (uint32_t *buff, size_t len, float percentile)
{
  unsigned int idx = (len * percentile);
  return buff[idx];
}

static inline void
reset ()
{
  i = 0;
}

static void
print (void)
{
  qsort (samples, RUNS, sizeof (samples[0]), cmp);

  uint64_t i = RUNS;
  uint64_t sum = 0;
  while (i--)
    sum += samples[i];

  printf ("Min: %u\n"
          "Max: %u\n"
          "Avg: %lu\n"
          "Median: %u\n"
          "99%%: %u\n",
          samples[0], samples[RUNS - 1], sum / RUNS,
          percentile (samples, RUNS, 0.50f),
          percentile (samples, RUNS, 0.99f));

  reset ();
}

static void
f1 (void)
{
  puts ("\nUsing raise");
  uint64_t i = RUNS;
  while (i--)
    {
      ts = rdtsc_start ();
      raise (SIGUSR1);
    }

  print ();
}

static void
f2 (void)
{
  puts ("\nUsing kill");

  pid_t pid = getpid ();
  uint64_t i = RUNS;
  while (i--)
    {
      ts = rdtsc_start ();
      kill (pid, SIGUSR1);
    }

  print ();
}

static void
f3 (void)
{
  puts ("\nUsing sigqueue");

  union sigval sv = { .sival_int = 0 };
  pid_t pid = getpid ();
  uint64_t i = RUNS;
  while (i--)
    {
      ts = rdtsc_start ();
      sigqueue (pid, SIGUSR1, sv);
    }

  print ();
}

static void
f4 (void)
{
  puts ("\nUsing tgkill");

  pid_t tgid, tid;
  tgid = tid = gettid (); // single thread
  uint64_t i = RUNS;
  while (i--)
    {
      ts = rdtsc_start ();
      tgkill (tgid, tid, SIGUSR1);
    }

  print ();
}

int
main (int argc, char **argv)
{
  sigset_t sg;
  sigfillset (&sg);
  struct sigaction act
      = { .sa_sigaction = handler, .sa_mask = sg, .sa_flags = SA_SIGINFO };
  sigaction (SIGUSR1, &act, NULL);

  printf ("Runs %lu\n", RUNS);

  f1 (); // warmup
  f1 ();
  f2 ();
  f3 ();
  f4 ();

  return 0;
}
