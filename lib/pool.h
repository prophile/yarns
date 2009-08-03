#ifndef __YARNS_POOL__
#define __YARNS_POOL__

typedef struct _pool pool;

// NB: pools are ATOMIC! :D

pool* pool_create ( unsigned long size, unsigned long slots );
void pool_destroy ( pool* p );

void* pool_allocate ( pool* p );
void pool_free ( pool* p, void* ptr );

#endif
