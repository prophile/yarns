#include "queue_atomic.h"
#include "atomic.h"
#include "alloc.h"
#include <assert.h>

struct _yarns_atomic_queue
{
	unsigned long max;
	unsigned long pushIndex;
	unsigned long popIndex;
	void** values;
};

yarns_atomic_queue* yarns_atomic_queue_new ( unsigned long slots )
{
	yarns_atomic_queue* q = (yarns_atomic_queue*)yalloc(sizeof(yarns_atomic_queue));
	q->values = yalloc(sizeof(void*)*slots);
	q->max = slots;
	q->pushIndex = 0;
	q->popIndex = 0;
	return q;
}

void yarns_atomic_queue_delete ( yarns_atomic_queue* queue )
{
	yfree(queue->values);
	yfree(queue);
}

void yarns_atomic_queue_push ( yarns_atomic_queue* queue, void* value )
{
	unsigned long push = atomic_add(&(queue->pushIndex), 1) - 1;
	push %= queue->max;
	queue->values[push] = value;
	assert((queue->pushIndex % queue->max) != (queue->popIndex % queue->max));
}

bool yarns_atomic_queue_pop ( yarns_atomic_queue* queue, void** value )
{
	unsigned long idx = atomic_add(&(queue->popIndex), 1) - 1;
	if (queue->pushIndex == queue->popIndex)
		return false;
	idx %= queue->max;
	return queue->values[idx];
}
