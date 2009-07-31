#ifndef __YARNS_CONTEXT__
#define __YARNS_CONTEXT__

#ifdef __MACH__
#define _XOPEN_SOURCE
#endif

#include <ucontext.h>
#include <stdbool.h>

#define YARN_CONTEXT_API "native-ucontext"

typedef ucontext_t yarn_context_t;

inline static bool yarn_context_init ( yarn_context_t* context )
	{ return getcontext(context) == 0; }
inline static bool yarn_context_set ( yarn_context_t* context )
	{ return setcontext(context) == 0; }
inline static bool yarn_context_set_stack ( yarn_context_t* context, void* stack, unsigned long size )
	{ context->uc_stack.ss_size = size; context->uc_stack.ss_sp = stack; context->uc_stack.ss_flags = 0; return 1; }
bool yarn_context_make ( yarn_context_t* context, void (*fn)(void*), void* udata );
inline static bool yarn_context_swap ( yarn_context_t* old, yarn_context_t* new )
	{ return swapcontext(old, new); }


#endif
