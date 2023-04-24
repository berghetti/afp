
// See section 3.2.1 from
// https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/ia-32-ia-64-benchmark-code-execution-paper.pdf

#include <stdint.h>

static inline uint64_t
rdtsc_start (void)
{
  uint32_t hi, low;
  asm volatile ("cpuid\n\t"
                "rdtsc\n\t"
                : "=a"(low), "=d"(hi)
                :
                : "rbx", "rcx");

  return ((uint64_t)hi << 32) | low;
}

static inline uint64_t
rdtsc_end (void)
{
  uint32_t hi, low;
  asm volatile ("rdtscp\n\t"
                "mov %%eax, %0\n\t"
                "mov %%edx, %1\n\t"
                "cpuid\n\t"
                : "=r"(low), "=r"(hi)
                :
                : "rax", "rdx", "rbx", "rcx");

  return ((uint64_t)hi << 32) | low;
}
