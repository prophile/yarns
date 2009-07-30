#include "context.h"

bool yarn_context_make ( yarn_context_t* context, void (*fn)(void*), void* udata )
{
	context->uc_link = 0;
	makecontext(context, (void (*)())fn, 1, udata);
	return 1;
}
