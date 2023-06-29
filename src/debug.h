
#ifndef ERROR_H
#define ERROR_H

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANSI_COLOR

#ifdef ANSI_COLOR
#define RED "\x1b[31;1m"
#define GREEN "\x1b[32;1m"
#define YELLOW "\x1b[33;1m"
#define BLUE "\x1b[34;1m"
#define MAGENTA "\x1b[35;1m"
#define RESET "\x1b[0;0m"

#define _INFO GREEN "[INFO] " RESET
#define _WARNING YELLOW "[WARNING] " RESET
#define _ERROR MAGENTA "[ERROR] " RESET
#define _FATAL RED "[FATAL] " RESET
#define _DEBUG BLUE "[DEBUG] " RESET
#else
#define _INFO "[INFO] "
#define _WARNING "[WARNING] "
#define _DEBUG "[DEBUG] "
#define _ERROR "[ERROR] "
#define _FATAL "[FATAL] "
#endif

void
print ( const char *, ... );

#define INFO( fmt, ... )                          \
  do                                              \
    {                                             \
      fprintf ( stdout, _INFO fmt, __VA_ARGS__ ); \
    }                                             \
  while ( 0 )

#define WARNING( fmt, ... )                          \
  do                                                 \
    {                                                \
      fprintf ( stdout, _WARNING fmt, __VA_ARGS__ ); \
    }                                                \
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
