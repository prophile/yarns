#ifndef __YARNS_CONTEXT__
#define __YARNS_CONTEXT__

#include <stdbool.h>

typedef struct _yarn_context yarn_context_t;

struct _yarn_context
{
	unsigned long address;
	unsigned long rbx, rbp, rsi, rsp;
	unsigned long param;
	void* stackPointer;
	unsigned long stackSize;
};

#define YARN_CONTEXT_API "yarn-context-x86_64"

bool yarn_context_init ( yarn_context_t* context );
bool yarn_context_set ( yarn_context_t* context );
inline static bool yarn_context_set_stack ( yarn_context_t* context, void* stack, unsigned long size )
	{ context->stackPointer = stack; context->stackSize = size; return 1; }
bool yarn_context_make ( yarn_context_t* context, void (*routine)(void*), void* udata );
bool yarn_context_swap ( yarn_context_t* oldContext, yarn_context_t* newContext );

#endif
