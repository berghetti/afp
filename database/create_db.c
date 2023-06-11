
// https://github.com/maxdml/shinjuku/blob/rocksdb/db/create_db.c

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "rocksdb/c.h"

#define DFL_NUM_ENTRIES 5000
#define DFL_DB_PATH "/tmp/my_db"

/*
 * argv[1] num_entries
 * argv[2] DB path */
int
main ( int argc, char **argv )
{
  int num_entries = ( argc >= 2 ) ? atoi ( argv[1] ) : DFL_NUM_ENTRIES;
  const char *DBPath = ( argc == 3 ) ? argv[2] : DFL_DB_PATH;

  rocksdb_t *db;
  rocksdb_backup_engine_t *be;
  rocksdb_options_t *options = rocksdb_options_create ();
  // Optimize RocksDB. This is the easiest way to
  // get RocksDB to perform well
  long cpus = sysconf ( _SC_NPROCESSORS_ONLN );  // get # of online cores
  rocksdb_options_increase_parallelism ( options, ( int ) ( cpus ) );
  rocksdb_options_optimize_level_style_compaction ( options, 0 );
  // create the DB if it's not already present
  rocksdb_options_set_create_if_missing ( options, 1 );

  // open DB
  char *err = NULL;
  db = rocksdb_open ( options, DBPath, &err );
  assert ( !err );

  char key[16], value[16], *value_ret;
  size_t len;

  // Put key-value
  rocksdb_writeoptions_t *writeoptions = rocksdb_writeoptions_create ();
  for ( int i = 0; i < num_entries; i++ )
    {
      snprintf ( key, sizeof key, "k%d", i );
      snprintf ( value, sizeof value, "v%d", i );
      rocksdb_put ( db,
                    writeoptions,
                    key,
                    strlen ( key ),
                    value,
                    strlen ( value ) + 1,
                    &err );
      assert ( !err );
    }

  // test
  // get value
  // rocksdb_readoptions_t *readoptions = rocksdb_readoptions_create ();
  // for ( int i = 0; i < num_entries; i++ )
  //  {
  //    snprintf ( key, sizeof key, "k%d", i );
  //    value_ret =
  //            rocksdb_get ( db, readoptions, key, strlen ( key ), &len, &err
  //            );
  //    assert ( !err );
  //    snprintf ( value, sizeof value, "v%d", i );
  //    assert ( strcmp ( value, value_ret ) == 0 );
  //    printf ( "key:%s value:%s\n", key, value_ret );
  //  }

  // test iterator
  // rocksdb_iterator_t *iter = rocksdb_create_iterator ( db, readoptions );
  // rocksdb_iter_seek_to_first ( iter );
  // while ( rocksdb_iter_valid ( iter ) )
  //  {
  //    const char *ikey = rocksdb_iter_key ( iter, &len );
  //    value_ret = rocksdb_get ( db, readoptions, ikey, len, &len, &err );

  //    assert ( !err );
  //    printf ( "key:%s value:%s\n", ikey, value_ret );
  //    rocksdb_iter_next ( iter );
  //  }
  // rocksdb_readoptions_destroy ( readoptions );

  // cleanup
  rocksdb_writeoptions_destroy ( writeoptions );
  rocksdb_options_destroy ( options );
  rocksdb_close ( db );

  printf ( "Database created on %s with %d entries\n", DBPath, num_entries );

  return 0;
}
