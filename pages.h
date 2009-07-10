#ifndef __YARNS_PAGES__
#define __YARNS_PAGES__

void* page_allocate ( unsigned long bytes );
void page_deallocate ( void* ptr, unsigned long bytes );

#endif
