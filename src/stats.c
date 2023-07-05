

#include <rte_ethdev.h>
#include <signal.h>

#include "compiler.h"
#include "debug.h"
#include "globals.h"

uint64_t swaps, interruptions, int_no_swaps, invalid_interruptions, yields,
        stealing;

static void
dpdk_stats ( uint32_t portid )
{
  struct rte_eth_stats eth_stats;
  int retval = rte_eth_stats_get ( portid, &eth_stats );
  if ( retval != 0 )
    {
      rte_exit ( EXIT_FAILURE, "Unable to get stats from portid\n" );
    }

  INFO ( "DPDK RX Stats:\n"
         "  ipackets: %lu\n"
         "  ibytes: %lu\n"
         "  ierror: %lu\n"
         "  imissed: %lu\n"
         "  rxnombuf: %lu\n",
         eth_stats.ipackets,
         eth_stats.ibytes,
         eth_stats.ierrors,
         eth_stats.imissed,
         eth_stats.rx_nombuf );

  INFO ( "DPDK TX Stats:\n"
         "  opackets: %lu\n"
         "  obytes: %lu\n"
         "  oerror: %lu\n",
         eth_stats.opackets,
         eth_stats.obytes,
         eth_stats.oerrors );

  struct rte_eth_xstat *xstats;
  struct rte_eth_xstat_name *xstats_names;

  int len = rte_eth_xstats_get ( portid, NULL, 0 );
  if ( len < 0 )
    {
      rte_exit ( EXIT_FAILURE,
                 "rte_eth_xstats_get(%u) failed: %d",
                 portid,
                 len );
    }

  xstats = calloc ( len, sizeof ( *xstats ) );
  if ( xstats == NULL )
    {
      rte_exit ( EXIT_FAILURE, "Failed to calloc memory for xstats" );
    }

  int ret = rte_eth_xstats_get ( portid, xstats, len );
  if ( ret < 0 || ret > len )
    {
      free ( xstats );
      rte_exit ( EXIT_FAILURE,
                 "rte_eth_xstats_get(%u) len%i failed: %d",
                 portid,
                 len,
                 ret );
    }

  xstats_names = calloc ( len, sizeof ( *xstats_names ) );
  if ( xstats_names == NULL )
    {
      free ( xstats );
      rte_exit ( EXIT_FAILURE, "Failed to calloc memory for xstats_names" );
    }

  ret = rte_eth_xstats_get_names ( portid, xstats_names, len );
  if ( ret < 0 || ret > len )
    {
      free ( xstats );
      free ( xstats_names );
      rte_exit ( EXIT_FAILURE,
                 "rte_eth_xstats_get_names(%u) len%i failed: %d",
                 portid,
                 len,
                 ret );
    }

  INFO ( "PORT %u statistics:\n", portid );
  for ( int i = 0; i < len; i++ )
    {
      if ( xstats[i].value > 0 )
        printf ( "  %-18s: %" PRIu64 "\n",
                 xstats_names[i].name,
                 xstats[i].value );
    }

  free ( xstats );
  free ( xstats_names );
}

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

  dpdk_stats ( 0 );

  exit ( 0 );
}

void
stats_init ( void )
{
  if ( SIG_ERR == signal ( SIGINT, statistics ) )
    FATAL ( "%s\n", "Error stats_init" );
}
