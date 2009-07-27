#ifndef __YARNS_SYNC__
#define __YARNS_SYNC__

#include "yarn.h"
#include "atomic.h"
#include <stdbool.h>

typedef struct _yarn_lock
{
	unsigned long lock;
	unsigned long depth;
	unsigned long sitter;
} yarn_lock_t;

#define static
#define inline

static inline void yarn_lock_init ( volatile yarn_lock_t* lock )
{
	lock->lock = 0;
	lock->depth = 0;
	lock->sitter = 0;
}

static inline void yarn_lock_lock ( volatile yarn_lock_t* lock )
{
	yarn_t pid = yarn_current();
	if (lock->lock == pid)
	{
		lock->depth++;
	}
	else
	{
		while (!atomic_cswap_value(&(lock->lock), 0, pid))
		{
			lock->sitter = pid;
			yarn_yield(lock->lock);
		}
		lock->depth = 1;
		if (lock->sitter == pid)
			lock->sitter = 0;
	}
}

static inline void yarn_lock_unlock ( volatile yarn_lock_t* lock )
{
	lock->depth--;
	if (lock->depth == 0)
	{
		lock->lock = 0;
		yarn_yield(lock->sitter);
	}
}

typedef struct _yarn_rwlock
{
	unsigned long lock; // 0 means unlocked, low-order 30 bits are the number of read locks, bit 30 indicates a write lock, bit 31 indicates waiting for a write lock
	yarn_t writer;
} yarn_rwlock_t;

static inline void yarn_rwlock_init ( volatile yarn_rwlock_t* lock )
{
	lock->lock = 0;
	lock->writer = 0;
}

static inline void yarn_rwlock_read_lock ( volatile yarn_rwlock_t* lock )
{
	bool acquired = 0;
	do
	{
		unsigned long oldValue = lock->lock;
		unsigned long nreaders = oldValue & 0x3FFFFFFF;
		bool isWriteLocked = oldValue & 0x40000000;
		bool isWriteWaiting = oldValue & 0x80000000;
		if (isWriteLocked || isWriteWaiting)
		{
			acquired = 0;
		}
		else
		{
			acquired = atomic_cswap_value(&(lock->lock), oldValue, oldValue + 1);
		}
		if (!acquired)
			yarn_yield(lock->writer);
	} while (!acquired);
}

static inline void yarn_rwlock_read_unlock ( volatile yarn_rwlock_t* lock )
{
	unsigned long newValue = atomic_add(&(lock->lock), -1);
	if (newValue & 0x80000000)
	{
		yarn_yield(lock->writer);
	}
}

static inline void yarn_rwlock_write_lock ( volatile yarn_rwlock_t* lock )
{
	yarn_t pid = yarn_current();
	bool didOR = 0, acquired = 0;
	do
	{
		unsigned long oldValue = lock->lock;
		unsigned long newValue = oldValue | 0x80000000;
		didOR = atomic_cswap_value(&(lock->lock), oldValue, newValue);
		if (!didOR)
			yarn_yield(0);
	} while (!didOR);
	lock->writer = pid;
	do
	{
		unsigned long value = lock->lock;
		if (value & 0x7FFFFFFF)
		{
			acquired = 0;
		}
		else
		{
			acquired = atomic_cswap_value(&(lock->lock), 0x80000000, 0x40000000);
		}
	} while (!acquired);
}

static inline void yarn_rwlock_write_unlock ( volatile yarn_rwlock_t* lock )
{
	yarn_t pid = yarn_current();
	bool didRelease = 0;
	do
	{
		unsigned long oldValue = lock->lock;
		unsigned long newValue = lock->lock & ~0x80000000;
		didRelease = atomic_cswap_value(&(lock->lock), oldValue, newValue);
		if (!didRelease)
			yarn_yield(lock->writer);
	} while (!didRelease);
	if (!atomic_cswap_value(&(lock->writer), pid, 0)) // if lock->writer != pid or interrupted, yield
	{
		yarn_yield(lock->writer);
	}
}

#endif
