#ifndef COMPILER_H
#define COMPILER_H

#define CACHE_LINE_SIZE 64

#define __noreturn __attribute__ ( ( noreturn ) )
#define __packed __attribute__ ( ( packed ) )
#define __notused __attribute__ ( ( unused ) )
#define __aligned( x ) __attribute__ ( ( aligned ( x ) ) )

#endif
