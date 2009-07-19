#include "queue.h"
#include "alloc.h"

// why 61, I hear you think
// it's 64 minus three - the number of extra elements in the struct
// so that each segment is a lovely round number of bytes in length
#define FIFO_ELEMENTS_PER_SEGMENT 61

typedef struct _fifo_segment fifo_segment;

struct _fifo
{
	unsigned long length;
	fifo_segment* head;
	fifo_segment* tail;
};

struct _fifo_segment
{
	unsigned long index;
	unsigned long count;
	fifo_segment* next;
	unsigned long elements[FIFO_ELEMENTS_PER_SEGMENT];
};

fifo* fifo_new ()
{
	fifo* f = (fifo*)yalloc(sizeof(fifo));
	return f;
}

void fifo_free ( fifo* q )
{
	fifo_segment* seg;
	while (q->head)
	{
		seg = q->head;
		q->head = q->head->next;
		yfree(seg);
	}
	yfree(q);
}

unsigned long fifo_length ( fifo* q )
{
	return q->length;
}

void fifo_enqueue ( fifo* q, unsigned long val )
{
	if (!q->head)
	{
		q->head = q->tail = (fifo_segment*)yalloc(sizeof(fifo_segment));
		q->head->count = 1;
		q->head->elements[0] = val;
	}
	else
	{
		if (q->tail->count == FIFO_ELEMENTS_PER_SEGMENT)
		{
			q->tail->next = (fifo_segment*)yalloc(sizeof(fifo_segment));
			q->tail = q->tail->next;
			q->tail->count = 1;
			q->tail->elements[0] = val;
		}
		else
		{
			q->tail->elements[q->tail->count++] = val;
		}
	}
	q->length++;
}

unsigned long fifo_dequeue ( fifo* q )
{
	unsigned long val;
	fifo_segment* seg;
	if (q->length == 0) return 0;
	val = q->head->elements[q->head->index++];
	if (q->head->index == FIFO_ELEMENTS_PER_SEGMENT)
	{
		seg = q->head;
		q->head = q->head->next;
		yfree(seg);
		if (!q->head) q->tail = 0;
	}
}
