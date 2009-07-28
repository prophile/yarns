#ifndef __YARNS_QUEUE_ATOMIC__
#define __YARNS_QUEUE_ATOMIC__

#include <stdbool.h>

typedef struct _atomic_queue atomic_queue;

atomic_queue* atomic_queue_new ( unsigned long slots );
void atomic_queue_delete ( atomic_queue* queue );

void atomic_queue_push ( atomic_queue* queue, void* value );
inline static void atomic_queue_push_integer ( atomic_queue* queue, unsigned long amount )
	{ atomic_queue_push(queue, (void*)amount); }

bool atomic_queue_pop ( atomic_queue* queue, void** value );
inline static bool atomic_queue_pop_integer ( atomic_queue* queue, unsigned long* value )
	{ return atomic_queue_pop(queue, (void**)value); }

#endif
