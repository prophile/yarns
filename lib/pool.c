#include "pool.h"
#include "alloc.h"
#include "atomic.h"

typedef struct _subpool subpool;

struct _pool
{
	unsigned long size;
	unsigned long nsubpools;
	subpool* subpools;
};

struct _subpool
{
	unsigned long availFlags;
	unsigned char* data;
};

static inline unsigned long clz ( unsigned long i )
{
	return __builtin_clzl(i);
}

#define SLOTS_PER_SUBPOOL sizeof(unsigned long)*8

pool* pool_create ( unsigned long size, unsigned long slots )
{
	int i;
	pool* p;
	unsigned char* data;
	unsigned long sizeTotal;
	unsigned long nsubpools = slots + SLOTS_PER_SUBPOOL - 1;
	nsubpools /= SLOTS_PER_SUBPOOL;
	sizeTotal = sizeof(pool) + sizeof(subpool)*nsubpools + size*SLOTS_PER_SUBPOOL*nsubpools;
	data = yalloc(sizeTotal);
	p = (pool*)data;
	data += sizeof(pool);
	p->size = size;
	p->nsubpools = nsubpools;
	p->subpools = (subpool*)data;
	data += sizeof(subpool)*nsubpools;
	for (i = 0; i < nsubpools; i++)
	{
		p->subpools[i].availFlags = ~0UL;
		p->subpools[i].data = data;
		data += SLOTS_PER_SUBPOOL*size;
	}
}

void pool_destroy ( pool* p )
{
	yfree(p);
}

void* pool_allocate ( pool* p )
{
	unsigned long availFlags, leadingZeroes, mask, newFlags;
	int i;
	for (i = 0; i < p->nsubpools; i++)
	{
		if ((availFlags = p->subpools[i].availFlags) != 0)
		{
			leadingZeroes = clz(availFlags);
			mask = 1 << (SLOTS_PER_SUBPOOL - leadingZeroes);
			newFlags = availFlags & ~mask;
			if (!atomic_cswap_value(&(p->subpools[i].availFlags), availFlags, newFlags))
				return pool_allocate(p); // restart the process. This is tail-recursive so we don't have to worry about stack overflows
			return (void*)(p->subpools[i].data + (p->size * leadingZeroes));
		}
	}
	return 0;
}

void pool_free ( pool* p, void* ptr )
{
	unsigned char* cptr = (unsigned char*)ptr;
	int i;
	if (ptr == 0)
		return; // early-exit for null pointers.
	for (i = 0; i < p->nsubpools; i++)
	{
		if (cptr > p->subpools[i].data)
		{
			unsigned long diff = cptr - p->subpools[i].data;
			if (diff < SLOTS_PER_SUBPOOL*p->size)
			{
				unsigned long mask, availFlags, newFlags;
				diff /= p->size;
				mask = (1 << (SLOTS_PER_SUBPOOL - diff));
				do
				{
					availFlags = p->subpools[i].availFlags;
					newFlags = availFlags | mask;
				} while(!atomic_cswap_value(&(p->subpools[i].availFlags), availFlags, newFlags));
				return;
			}
		}
	}
}
