

#include <stdarg.h>  // va_*
#include <stdio.h>

#include "debug.h"

/*
 * vfprintf() in print() uses nonliteral format string. It may break
 * compilation if user enables corresponding warning. Disable it explicitly.
 */
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

void
print ( const char *restrict fmt, ... )
{
  va_list args;

  va_start ( args, fmt );
  vfprintf ( stderr, fmt, args );
  va_end ( args );
}
