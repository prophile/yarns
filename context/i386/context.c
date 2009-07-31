#include "context.h"
#include <stdlib.h>

extern int gctx ( yarn_context_t* ctx ) __asm__("___ctx_gctx");
extern void sctx ( yarn_context_t* ctx ) __asm__("___ctx_sctx");
extern void start ( yarn_context_t* ctx, void* param, void (*routine)(void*) ) __asm__("___ctx_start");

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
	stack_top -= sizeof(intptr_t);
	
	// adjust top of stack
	stack_top = stack_top - (sizeof(unsigned long) * 2);
	stack_top = (unsigned char*)((unsigned long)stack_top & ~15);
	stack_top = stack_top - (2 * sizeof(unsigned long));
	argp = (unsigned long*)stack_top;
	
	// write stack
	*(argp++) = (unsigned long)start;
	*(argp++) = (unsigned long)routine;
	*(argp++) = (unsigned long)udata;
	*(argp++) = (unsigned long)context;
	
	// write registers
	context->esi = (unsigned long)argp;
	context->ebp = (unsigned long)0;
	context->esp = (unsigned long)(stack_top + sizeof(void*));
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
