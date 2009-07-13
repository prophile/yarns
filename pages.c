#include "pages.h"
#include <assert.h>
#include <stdio.h>
#include "config.h"
#if YARNS_SELECTED_TARGET == YARNS_TARGET_MACH
#include <mach/mach.h>

void* page_allocate ( unsigned long bytes )
{
	kern_return_t rt;
	void* pointer = 0;
	//printf("Allocating %d bytes = %d pages\n", bytes, bytes / 4096);
	assert(!(bytes % 4096));
	rt = vm_allocate(mach_task_self(), (vm_address_t*)&pointer, bytes, 1);
	return pointer;
}

void page_deallocate ( void* ptr, unsigned long bytes )
{
	assert(!(bytes % 4096));
	vm_deallocate(mach_task_self(), (vm_address_t)ptr, bytes);
}
#elif YARNS_SELECTED_TARGET == YARNS_TARGET_LINUX
#include <stdlib.h>

void* page_allocate ( unsigned long bytes )
{
	return valloc(bytes);
}

void page_deallocate ( void* ptr, unsigned long bytes )
{
	(void)bytes;
	free(ptr);
}
#else
#error Page allocation routines not defined for this target.
#endif
