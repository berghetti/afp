

#include <signal.h>

#include "compiler.h"
#include "debug.h"
#include "globals.h"

uint64_t swaps, interruptions, int_no_swaps, invalid_interruptions, yields,
        stealing;

static void
statistics ( int __notused sig )
{
  INFO ( "Statistics:\n"
         "  Interruptions:         %lu\n"
         "  Context swaps:         %lu\n"
         "  Long continue:         %lu\n"
         "  Invalid interruptions: %lu\n"
         "  Yields:                %lu\n"
         "  Work stealing:         %lu\n",
         interruptions,
         swaps,
         int_no_swaps,
         invalid_interruptions,
         yields,
         stealing );

  exit ( 0 );
}

void
stats_init ( void )
{
  if ( SIG_ERR == signal ( SIGINT, statistics ) )
    FATAL ( "%s\n", "Error stats_init" );
}
