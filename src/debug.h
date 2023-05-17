
#ifndef ERROR_H
#define ERROR_H

#include <errno.h>  // variable errno
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // strerror

#define _INFO "[INFO] "
#define _DEBUG "[DEBUG] "
#define _ERROR "[ERROR] "
#define _FATAL "[FATAL] "

void
print ( const char *, ... );

#define INFO( fmt, ... )                          \
  do                                              \
    {                                             \
      fprintf ( stderr, _INFO fmt, __VA_ARGS__ ); \
    }                                             \
  while ( 0 )

#define ERROR( fmt, ... )                          \
  do                                               \
    {                                              \
      fprintf ( stderr, _ERROR fmt, __VA_ARGS__ ); \
    }                                              \
  while ( 0 )

#define FATAL( fmt, ... )                          \
  do                                               \
    {                                              \
      fprintf ( stderr, _FATAL fmt, __VA_ARGS__ ); \
      exit ( EXIT_FAILURE );                       \
    }                                              \
  while ( 0 )

#ifdef NDEBUG
#define DEBUG( fmt, ... )
#define DEBUG_ARRAY( a, size )
#else
#define DEBUG( fmt, ... )                                               \
  do                                                                    \
    {                                                                   \
      print ( _DEBUG "%s:%d - " fmt, __FILE__, __LINE__, __VA_ARGS__ ); \
    }                                                                   \
  while ( 0 )

#define DEBUG_ARRAY( a, size )                   \
  ( {                                            \
    for ( int i = 0; i < ( int ) ( size ); ++i ) \
      {                                          \
        fprintf ( stderr, "%#x ", a[i] );        \
      }                                          \
    putchar ( '\n' );                            \
  } )
#endif

#endif  // ERROR_H
