#include "context.h"
#include <stdlib.h>

extern long gctx ( yarn_context_t* ctx ) __asm__("___ctx_gctx");
extern void sctx ( yarn_context_t* ctx ) __asm__("___ctx_sctx");
extern void start ( void* param, void (*routine)(void*) ) __asm__("___ctx_start");

bool yarn_context_init ( yarn_context_t* oldContext )
{
	gctx(oldContext);
	return 1;
}

bool yarn_context_set ( yarn_context_t* context )
{
	sctx(context);
	return 0;
}

bool yarn_context_make ( yarn_context_t* context, void (*routine)(void*), void* udata )
{
	unsigned char* stack_top;
	unsigned long* argp;
	stack_top = (unsigned char*)context->stackPointer;
	stack_top += context->stackSize;

	// add space for previous stack frame
	stack_top -= sizeof(unsigned long) * 2;
	stack_top = (unsigned char*)((unsigned long)stack_top & ~15ULL); // realign the stack to 16 bytes

	// write registers
	context->rbp = (unsigned long)stack_top;
	context->r12 = (unsigned long)udata;
	context->r13 = (unsigned long)routine;
	context->address = (unsigned long)start;
	
	return 0;
}

bool yarn_context_swap ( yarn_context_t* oldContext, yarn_context_t* newContext )
{
	int rc = gctx(oldContext);
	if (rc == 0)
		sctx(newContext);
	return 1;
}
