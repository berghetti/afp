

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "hello.h"
#include "trap.h"

static volatile int worker_ready = 0, worker_stop = 2;

void
jmp_for_me ( void )
{
  char a[8192] = { 0 };
  asm volatile( "mov $3, %rax\n\t"
                "mov $3, %rdi\n\t"
                "mov $3, %rsi\n\t"
                "mov $3, %rdx\n\t"
                "mov $3, %rcx\n\t"
                "mov $3, %r8\n\t"
                "mov $3, %r9\n\t"
                "mov $3, %r10\n\t"
                "mov $3, %r11\n\t"
                "mov $3, %rbx\n\t"
                "mov $3, %r12\n\t"
                "mov $3, %r13\n\t"
                "mov $3, %r14\n\t"
                "mov $3, %r15\n\t" );

  puts ( "Uhul" );
  worker_stop = 1;
}

void *
worker ( void *arg )
{

  // char a[512];

  // register int rax asm( "rax" ) = 42;
  asm volatile( "mov $5, %rax\n\t"
                "mov $5, %rdi\n\t"
                "mov $5, %rsi\n\t"
                "mov $5, %rdx\n\t"
                "mov $5, %rcx\n\t"
                "mov $5, %r8\n\t"
                "mov $5, %r9\n\t"
                "mov $5, %r10\n\t"
                "mov $5, %r11\n\t"
                "mov $5, %rbx\n\t"
                "mov $5, %r12\n\t"
                "mov $5, %r13\n\t"
                "mov $5, %r14\n\t"
                "mov $5, %r15\n\t" );

  // label:
  // printf ( "label: %p\n", &&label );
  worker_ready = 1;
  while ( worker_stop == 2 )
    ;

  return NULL;
}

int
main ( void )
{
  cpu_set_t cpus;
  CPU_ZERO ( &cpus );
  CPU_SET ( 0, &cpus );
  if ( pthread_setaffinity_np ( pthread_self (), sizeof ( cpu_set_t ), &cpus ) )
    {
      printf ( "Could not pin thread to core %d.\n", 0 );
      return 1;
    }

  CPU_ZERO ( &cpus );
  CPU_SET ( 1, &cpus );
  pthread_t tid;
  pthread_attr_t attr;
  pthread_attr_init ( &attr );
  pthread_attr_setaffinity_np ( &attr, sizeof ( cpu_set_t ), &cpus );
  pthread_create ( &tid, &attr, worker, NULL );

  int fd = open ( DEVICE_PATH, O_RDWR );
  if ( fd < 0 )
    {
      perror ( "Error to open character " DEVICE_PATH );
      return 1;
    }

  while ( !worker_ready )
    ;

  printf ( "%p\n", jmp_for_me );

  // int r = ioctl ( fd, KMOD_IPI_SEND, jmp_for_me );
  int r = ioctl ( fd, KMOD_IPI_SEND, trap_entry );
  if ( r < 0 )
    {
      perror ( "Error to send ioctl " DEVICE_PATH );
      return 1;
    }

  close ( fd );

  pthread_join ( tid, NULL );

  // while ( 1 )
  //  ;

  return 0;
}
