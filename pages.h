#ifndef __YARNS_PAGES__
#define __YARNS_PAGES__

#define PAGE_ZEROFILL 0x00000001
#define PAGE_SECURE   0x00000002
#define PAGE_SHARE    0x00000004

#define PAGE_READ    0x00000001
#define PAGE_WRITE   0x00000002
#define PAGE_EXECUTE 0x00000004

void* page_allocate ( unsigned long bytes, unsigned options, unsigned permissions );
void page_permissions ( void* ptr, unsigned long bytes, unsigned permissions );
void page_deallocate ( void* ptr, unsigned long bytes );

#endif
