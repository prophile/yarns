#ifndef __YARNS_ALLOC__
#define __YARNS_ALLOC__

#include "config.h"

#ifdef YARNS_USE_SYSTEM_ALLOCATOR

#include <stdlib.h>

#define yalloc(l) calloc(1,l)
#define yfree free

#else

void* yalloc ( unsigned long len ) __attribute__((malloc));
void yfree ( void* ptr );

#endif

#endif
