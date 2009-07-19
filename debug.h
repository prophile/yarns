#ifndef __YARNS_DEBUG__
#define __YARNS_DEBUG__

#include <stdio.h>

#define DEBUG_ALLOCATOR      0x0001
#define DEBUG_SCHEDULER      0x0002
#define DEBUG_SMP_SCHEDULER  0x0004
#define DEBUG_YARNS          0x0008
#define DEBUG_PAGE_ALLOCATOR 0x0010

#define DEBUG_EVERYTHING ~0

#define DEBUG_WHAT DEBUG_SCHEDULER|DEBUG_SMP_SCHEDULER

#define DEBUG(format, args...) if (DEBUG_MODULE & (DEBUG_WHAT)) { fprintf(stderr, format , ## args); }

#endif
