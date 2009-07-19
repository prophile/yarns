#ifndef __YARNS_QUEUE__
#define __YARNS_QUEUE__

typedef struct _fifo fifo;

fifo* fifo_new ();
void fifo_free ( fifo* q );

unsigned long fifo_length ( fifo* q );
unsigned long fifo_dequeue ( fifo* q );
static inline void* fifo_dequeue_pointer ( fifo* q )
	{ return (void*)fifo_dequeue(q); }
void fifo_enqueue ( fifo* q, unsigned long val );
static inline void fifo_enqueue_pointer ( fifo* q, void* ptr )
	{ fifo_enqueue(q, (unsigned long)ptr); }

#endif
