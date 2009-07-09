#ifndef __YARNS_LOCK__
#define __YARNS_LOCK__

#include <unistd.h>

typedef unsigned long lock_t;

static inline void lock_init ( volatile lock_t* lock )
{
	*lock = 0;
}

static inline void lock_lock ( volatile lock_t* lock )
{
	while (!__sync_bool_compare_and_swap(lock, 0, 1))
		usleep(2);
}

static inline void lock_unlock ( volatile lock_t* lock )
{
	*lock = 0;
}

#endif
