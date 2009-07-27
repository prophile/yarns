#ifndef __YARNS_ATOMIC__
#define __YARNS_ATOMIC__

#include <stdbool.h>

#define ATOMIC_API_GCC 1
#define ATOMIC_API_OSATOMIC 2

#if defined(__ppc__)
#define ATOMIC_API ATOMIC_API_OSATOMIC
#else
#define ATOMIC_API ATOMIC_API_GCC
#endif

#if ATOMIC_API == ATOMIC_API_GCC

static inline bool atomic_cswap_value ( volatile unsigned long* ptr, unsigned long oldValue, unsigned long newValue )
{
	return __sync_bool_compare_and_swap(ptr, oldValue, newValue);
}

static inline bool atomic_cswap_pointer ( void* volatile* ptr, void* oldValue, void* newValue )
{
	return __sync_bool_compare_and_swap(ptr, oldValue, newValue);
}

static inline unsigned long atomic_add ( volatile unsigned long* ptr, long amount )
{
	return __sync_fetch_and_add(ptr, amount);
}

#else

#include <libkern/OSAtomic.h>

static inline bool atomic_cswap_value ( volatile unsigned long* ptr, unsigned long oldValue, unsigned long newValue )
{
#ifdef __LP64__
	return OSAtomicCompareAndSwap64(oldValue, newValue, (int64_t volatile*)ptr);
#else
	return OSAtomicCompareAndSwap32(oldValue, newValue, (int32_t volatile*)ptr);
#endif
}

static inline bool atomic_cswap_pointer ( void* volatile* ptr, void* oldValue, void* newValue )
{
	return OSAtomicCompareAndSwapPtrBarrier(oldValue, newValue, ptr);
}

static inline unsigned long atomic_add ( volatile unsigned long* ptr, long amount )
{
#ifdef __LP64__
	return OSAtomicAdd64Barrier(amount, (int64_t volatile*)ptr);
#else
	return OSAtomicAdd32Barrier(amount, (int32_t volatile*)ptr);
#endif
}

#endif

static inline unsigned long atomic_sub ( volatile unsigned long* ptr, long amount )
{
	return atomic_add(ptr, -amount);
}

#endif
