#ifndef __YARNS_LOCK__
#define __YARNS_LOCK__

#include <unistd.h>
#include "atomic.h"
#include <stdbool.h>

typedef unsigned long lock_t;

static inline void lock_init ( volatile lock_t* lock )
{
	*lock = 0;
}

static inline void lock_lock ( volatile lock_t* lock )
{
	while (!atomic_cswap_value(lock, 0, 1))
		usleep(2);
}

static inline bool lock_trylock ( volatile lock_t* lock )
{
	return atomic_cswap_value(lock, 0, 1);
}

static inline void lock_unlock ( volatile lock_t* lock )
{
	*lock = 0;
}

#endif
