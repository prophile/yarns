#include "queue_atomic.h"
#include "atomic.h"
#include "alloc.h"
#include <assert.h>

struct _atomic_queue
{
	unsigned long max;
	unsigned long pushIndex;
	unsigned long popIndex;
	void** values;
};

atomic_queue* atomic_queue_new ( unsigned long slots )
{
	atomic_queue* q = (atomic_queue*)yalloc(sizeof(atomic_queue));
	q->values = yalloc(sizeof(void*)*slots);
	q->max = slots;
	q->pushIndex = 0;
	q->popIndex = 0;
	return q;
}

void atomic_queue_delete ( atomic_queue* queue )
{
	yfree(queue->values);
	yfree(queue);
}

void atomic_queue_push ( atomic_queue* queue, void* value )
{
	unsigned long push = atomic_add(&(queue->pushIndex), 1) - 1;
	push %= queue->max;
	queue->values[push] = value;
	assert((queue->pushIndex % queue->max) != (queue->popIndex % queue->max));
}

bool atomic_queue_pop ( atomic_queue* queue, void** value )
{
	unsigned long idx = atomic_add(&(queue->popIndex), 1) - 1;
	if (queue->pushIndex == queue->popIndex)
		return false;
	idx %= queue->max;
	return queue->values[idx];
}
