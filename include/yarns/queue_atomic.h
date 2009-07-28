#ifndef __YARNS_QUEUE_ATOMIC__
#define __YARNS_QUEUE_ATOMIC__

#include <stdbool.h>

typedef struct _yarns_atomic_queue yarns_atomic_queue;

yarns_atomic_queue* yarns_atomic_queue_new ( unsigned long slots );
void yarns_atomic_queue_delete ( yarns_atomic_queue* queue );

void yarns_atomic_queue_push ( yarns_atomic_queue* queue, void* value );
inline static void yarns_atomic_queue_push_integer ( yarns_atomic_queue* queue, unsigned long amount )
	{ yarns_atomic_queue_push(queue, (void*)amount); }

bool yarns_atomic_queue_pop ( yarns_atomic_queue* queue, void** value );
inline static bool yarns_atomic_queue_pop_integer ( yarns_atomic_queue* queue, unsigned long* value )
	{ return yarns_atomic_queue_pop(queue, (void**)value); }

#endif
