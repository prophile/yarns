#ifndef __YARNS_ALLOC__
#define __YARNS_ALLOC__

void yallocinit ();
void* yalloc ( unsigned long len );
void yfree ( void* ptr );

#endif
