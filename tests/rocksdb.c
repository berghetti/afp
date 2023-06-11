
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>

#include <rocksdb/c.h>

#include "util.h"

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define DB_PATH "/tmp/my_db"
#define SAMPLES 10000UL

#define NDEBUG

static rocksdb_t *db;

static uint32_t samples[SAMPLES];

static void
do_get ( rocksdb_readoptions_t *readoptions )
{
  const char *retr_key;
  size_t len;
  char *err = NULL;

  rocksdb_iterator_t *iter = rocksdb_create_iterator ( db, readoptions );
  rocksdb_iter_seek_to_first ( iter );
  if ( rocksdb_iter_valid ( iter ) )
    {
      retr_key = rocksdb_iter_key ( iter, &len );
#ifndef NDEBUG
      char *value = rocksdb_get ( db, readoptions, retr_key, len, &len, &err );
      assert ( !err );
      printf ( "key:%s value:%s\n", retr_key, value );
#endif
    }
  rocksdb_iter_destroy ( iter );
}

static void
do_scan ( rocksdb_readoptions_t *readoptions )
{
  const char *retr_key;
  size_t len;
  char *err = NULL;

  rocksdb_iterator_t *iter = rocksdb_create_iterator ( db, readoptions );
  rocksdb_iter_seek_to_first ( iter );
  while ( rocksdb_iter_valid ( iter ) )
    {
      retr_key = rocksdb_iter_key ( iter, &len );
#ifndef NDEBUG
      char *value = rocksdb_get ( db, readoptions, retr_key, len, &len, &err );
      assert ( !err );
      printf ( "key:%s value:%s\n", retr_key, value );
#endif
      rocksdb_iter_next ( iter );
    }
  rocksdb_iter_destroy ( iter );
}

static void
print ( char *msg )
{
  qsort ( samples, SAMPLES, sizeof ( samples[0] ), cmp_uint32 );
  printf ( "\n%s\n"
           "  min:  %u\n"
           "  mean: %u\n"
           "  99%%: %u\n"
           "  99.9%%: %u\n"
           "  max:    %u\n",
           msg,
           samples[0],
           percentile ( samples, SAMPLES, 0.50f ),
           percentile ( samples, SAMPLES, 0.99f ),
           percentile ( samples, SAMPLES, 0.999f ),
           samples[SAMPLES - 1] );
}

void
bench ( void )
{

  uint64_t start;
  int i;

  i = SAMPLES;
  while ( i-- )
    {
      start = __rdtsc ();
      rocksdb_readoptions_t *readoptions = rocksdb_readoptions_create ();
      do_get ( readoptions );
      rocksdb_readoptions_destroy ( readoptions );
      samples[i] = __rdtsc () - start;
    }
  print ( "Benchmark GET" );

  i = SAMPLES;
  while ( i-- )
    {
      start = __rdtsc ();
      rocksdb_readoptions_t *readoptions = rocksdb_readoptions_create ();
      do_scan ( readoptions );
      rocksdb_readoptions_destroy ( readoptions );
      samples[i] = __rdtsc () - start;
    }
  print ( "Benchmark SCAN" );
}

#define DFL_DB_PATH "/tmp/my_db"

int
main ( int argc, char **argv )
{
  const char *DBPath = ( argc == 2 ) ? argv[1] : DFL_DB_PATH;

  // Initialize RocksDB
  rocksdb_options_t *options = rocksdb_options_create ();
  rocksdb_options_set_allow_mmap_reads ( options, 1 );
  rocksdb_options_set_allow_mmap_writes ( options, 1 );
  rocksdb_slicetransform_t *prefix_extractor =
          rocksdb_slicetransform_create_fixed_prefix ( 8 );
  rocksdb_options_set_prefix_extractor ( options, prefix_extractor );
  // rocksdb_options_set_plain_table_factory ( options, 0, 10, 0.75, 3 );
  // Optimize RocksDB. This is the easiest way to
  // get RocksDB to perform well
  rocksdb_options_increase_parallelism ( options, 0 );
  rocksdb_options_optimize_level_style_compaction ( options, 0 );
  // create the DB if it's not already present
  rocksdb_options_set_create_if_missing ( options, 1 );

  // open DB
  char *err = NULL;
  db = rocksdb_open ( options, DBPath, &err );
  if ( err )
    {
      fprintf ( stderr, "Error to open database:\n%s\n", err );
      return 1;
    }

  bench ();

  // cleanup
  // rocksdb_readoptions_destroy ( readoptions );
  rocksdb_options_destroy ( options );
  rocksdb_close ( db );
}
