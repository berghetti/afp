
/* Test overhead to send signals on same time from various threads.
 * Kernel taken lock to send signal, so us test misalign between threads
 * to send the signal to avoid taken the lock on same time.
 *
 * The 'us' parameter is the difference between threads starting o signal send.
 *
 */

#define _GNU_SOURCE // to CPU_SET
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "cycles_count.h"

#define RUNS 100000UL
static int num_threads;
static int us;

static pthread_barrier_t barrier;

static __thread volatile uint64_t ts;

static __thread uint64_t i;
static __thread uint32_t samples[RUNS];

static void
handler (int sig, siginfo_t *info, void *ucontext)
{
  samples[i++] = rdtsc_end () - ts;
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

static void
print (int thread)
{
  qsort (samples, RUNS, sizeof (samples[0]), cmp);

  uint64_t i = RUNS;
  uint64_t sum = 0;
  while (i--)
    sum += samples[i];

  printf ("\nThread %d\n"
          "  Min: %u\n"
          "  Avg: %lu\n"
          "  Median: %u\n"
          "  99%%: %u\n"
          "  99.9%%: %u\n"
          "  Max: %u\n",
          thread, samples[0], sum / RUNS, percentile (samples, RUNS, 0.50f),
          percentile (samples, RUNS, 0.99f),
          percentile (samples, RUNS, 0.999f), samples[RUNS - 1]);
}

static void *
f1 (void *arg)
{
  int id = (int)(uintptr_t)arg;

  cpu_set_t cpuset;
  CPU_ZERO (&cpuset);
  CPU_SET (id + 11, &cpuset); // match my home pc isolcpu
  pthread_setaffinity_np (pthread_self (), sizeof (cpu_set_t), &cpuset);

  pid_t tgid = getpid ();
  pid_t tid = gettid ();
  uint64_t count = RUNS;
  struct timespec req = { .tv_nsec = us * 1000 * id };

  // First execution not await others threads. Assume as warm up.
  while (count--)
    {
      ts = rdtsc_start ();
      tgkill (tgid, tid, SIGUSR1);

      pthread_barrier_wait (&barrier);
      nanosleep (&req, NULL); // ensure specific misalign between threads start
    }

  print (id);

  return 0;
}

int
main (int argc, char **argv)
{
  if (argc != 3)
    {
      fprintf (stderr, "Usage %s num_threads us\n", argv[0]);
      return 1;
    }

  num_threads = atoi (argv[1]);
  num_threads += (num_threads == 0);

  us = atoi (argv[2]);

  struct sigaction act = { .sa_sigaction = handler, .sa_flags = SA_SIGINFO };
  sigaction (SIGUSR1, &act, NULL);

  printf ("Threads:  %u\n"
          "microseconds: %u\n"
          "Samples: %lu\n",
          num_threads, us, RUNS);

  pthread_barrier_init (&barrier, NULL, num_threads);

  pthread_t ptids[num_threads];
  for (int i = 0; i < num_threads; i++)
    if (pthread_create (&ptids[i], NULL, f1, (void *)(uintptr_t)i))
      return 1;

  for (int i = 0; i < num_threads; i++)
    pthread_join (ptids[i], NULL);

  return 0;
}
