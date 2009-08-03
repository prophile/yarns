#include "pool.h"
#include "alloc.h"
#include "atomic.h"
#include "debug.h"
#include <assert.h>

#define DEBUG_MODULE DEBUG_POOL_ALLOCATOR

typedef struct _subpool subpool;

struct _pool
{
	unsigned long size;
	unsigned long slots;
	unsigned long nsubpools;
	subpool* subpools;
	pool* chain;
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
	assert(size);
	assert(slots);
	sizeTotal = sizeof(pool) + (sizeof(subpool)*nsubpools) + (size*SLOTS_PER_SUBPOOL*nsubpools);
	data = yalloc(sizeTotal);
	assert(data);
	p = (pool*)data;
	data += sizeof(pool);
	p->size = size;
	p->slots = slots;
	p->chain = 0;
	p->nsubpools = nsubpools;
	p->subpools = (subpool*)data;
	data += sizeof(subpool)*nsubpools;
	for (i = 0; i < nsubpools; i++)
	{
		p->subpools[i].availFlags = ~0UL;
		p->subpools[i].data = data;
		data += SLOTS_PER_SUBPOOL*size;
	}
	return p;
}

void pool_destroy ( pool* p )
{
	if (!p)
		return;
	if (p->chain)
		pool_destroy(p->chain); // not tail recursive, unfortunately, but shouldn't generally be very deep at all
	yfree(p);
}

static void* allocate ( pool* root, pool* p )
{
	unsigned long availFlags, leadingZeroes, mask, newFlags, bitPosition;
	int i;
	void* ptr;
	assert(p);
	for (i = 0; i < p->nsubpools; i++)
	{
		if ((availFlags = p->subpools[i].availFlags) != 0)
		{
			leadingZeroes = clz(availFlags);
			bitPosition = (SLOTS_PER_SUBPOOL - leadingZeroes) - 1;
			mask = 1 << bitPosition;
			newFlags = availFlags & ~mask;
			if (!atomic_cswap_value(&(p->subpools[i].availFlags), availFlags, newFlags))
				return allocate(root, root); // restart the process. This is tail-recursive so we don't have to worry about stack overflows
			ptr = (p->subpools[i].data + (p->size * bitPosition));
			DEBUG("pool %p allocated block %p\n", p, ptr);
			return ptr;
		}
	}
	if (p->chain)
	{
		return allocate(root, p->chain);
	}
	else
	{
		pool* chained = pool_create(p->size, p->slots);
		if (!atomic_cswap_pointer((void *volatile*)&(p->chain), 0, chained))
			pool_destroy(chained);
		return allocate(root, chained);
	}
}

void* pool_allocate ( pool* p )
{
	return allocate(p, p);
}

void pool_free ( pool* p, void* ptr )
{
	unsigned char* cptr = (unsigned char*)ptr;
	int i;
	assert(p);
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
	// not in this pool, if it's chained try the next in the chain
	if (p->chain)
		pool_free(p->chain, ptr);
}
