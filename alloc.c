#include "alloc.h"
#include "pages.h"
#include <stdlib.h>
#include "debug.h"
#include "lock.h"
#include <string.h>

#ifndef YARNS_USE_SYSTEM_ALLOCATOR

#define DEBUG_MODULE DEBUG_ALLOCATOR

typedef struct _bigalloc_book bigalloc_book;

struct _bigalloc_book
{
	unsigned long lptr;
	unsigned long len;
	bigalloc_book* next;
};

#define LITTLE_PAGES_COUNT 8
#define BIG_PAGE_THRESHOLD 4096
#define LITTLE_PAGES_SIZE (4096*LITTLE_PAGES_COUNT)

typedef struct _little_page_info little_page_info;

struct _little_page_info
{
	unsigned long nalloc;
	unsigned long remainingLen;
	unsigned char* nextbase;
};

typedef struct _little_block_info little_block_info;

struct _little_block_info
{
	unsigned long len;
};

#define BIG_BOOK_HASH_SIZE 256

static lock_t big_lock;
static lock_t little_lock;

static bigalloc_book books[BIG_BOOK_HASH_SIZE];
static unsigned char* little_page_active = NULL;

static void little_page_allocate ()
{
	little_page_info* lpi;
	little_page_active = page_allocate(LITTLE_PAGES_SIZE, PAGE_ZEROFILL, PAGE_READ | PAGE_WRITE);
	lpi = (little_page_info*)little_page_active;
	lpi->nalloc = 0;
	lpi->nextbase = little_page_active + sizeof(little_page_info);
	lpi->remainingLen = LITTLE_PAGES_SIZE - sizeof(little_page_info);
	DEBUG("little allocator claimed %d new pages\n", LITTLE_PAGES_COUNT);
}

static little_page_info* get_little_page ()
{
	little_page_info* lpi;
	if (!little_page_active)
	{
		little_page_allocate();
	}
	lpi = (little_page_info*)little_page_active;
	if (lpi->nextbase == 0)
	{
		little_page_allocate();
		lpi = (little_page_info*)little_page_active;
	}
	return lpi;
}

static void* little_alloc ( unsigned long len )
{
	unsigned char* ptr;
	little_page_info* lpi;
	lock_lock(&little_lock);
	lpi = get_little_page();
	if (lpi->remainingLen < len)
	{
		little_page_allocate();
		lpi = get_little_page();
	}
	ptr = lpi->nextbase;
	lpi->remainingLen -= len;
	lpi->nextbase += len;
	lpi->nalloc++;
	lock_unlock(&little_lock);
	return ptr;
}

static void little_free ( void* ptr )
{
	little_page_info* lpi;
	unsigned long lptr = (unsigned long)ptr;
	lptr -= (lptr % LITTLE_PAGES_SIZE);
	lpi = (little_page_info*)lptr;
	lock_lock(&little_lock);
	if (--lpi->nalloc == 0)
	{
		if ((unsigned char*)lpi == little_page_active)
		{
			DEBUG("little allocator current page now has no content, reusing\n");
			lpi->remainingLen = LITTLE_PAGES_SIZE - sizeof(little_page_info);
			lpi->nextbase = little_page_active + sizeof(little_page_info);
			memset(lpi->nextbase, 0, lpi->remainingLen);
		}
		else
		{
			DEBUG("little allocator freed %d pages\n", LITTLE_PAGES_COUNT);
			page_deallocate(ptr, LITTLE_PAGES_SIZE);
		}
	}
	lock_unlock(&little_lock);
}

void yallocinit ()
{
	int i;
	for (i = 0; i < BIG_BOOK_HASH_SIZE; i++)
	{
		books[i].lptr = 0;
		books[i].len = 0;
		books[i].next = 0;
	}
	lock_init(&big_lock);
	lock_init(&little_lock);
}

static void bigbook_insert ( unsigned long lptr, unsigned long len )
{
	unsigned idx = lptr;
	idx /= 4096;
	idx %= BIG_BOOK_HASH_SIZE;
	lock_lock(&big_lock);
	if (books[idx].lptr == 0)
	{
		// insert straight off
		books[idx].lptr = lptr;
		books[idx].len = len;
	}
	else
	{
		bigalloc_book* book = &books[idx];
		while (book->next)
			book = book->next;
		book->next = yalloc(sizeof(bigalloc_book));
		book = book->next;
		book->next = 0;
		book->lptr = lptr;
		book->len = len;
	}
	lock_unlock(&big_lock);
}

static unsigned long bigbook_eatlen ( unsigned long lptr )
{
	unsigned idx = lptr, len;
	bigalloc_book* book;
	idx /= 4096;
	idx %= BIG_BOOK_HASH_SIZE;
	lock_lock(&big_lock);
	book = &books[idx];
	while (book && book->lptr != lptr)
		book = book->next;
	if (book->lptr == lptr)
	{
		len = book->len;
		if (book != &books[idx])
		{
			// it was allocated, eat it
			bigalloc_book* secondBook = &books[idx];
			while (secondBook->next != book)
				secondBook = secondBook->next;
			secondBook->next = book->next;
			yfree(book);
		}
	}
	else
	{
		len = 0; // didn't find it... strange
	}
	lock_unlock(&big_lock);
	return len;
}

void* yalloc ( unsigned long len )
{
	void* ptr;
	unsigned long lptr;
	if (len > 2048)
	{
		len += 4095;
		len &= ~4095;
		ptr = page_allocate(len, PAGE_ZEROFILL, PAGE_READ | PAGE_WRITE);
		lptr = (unsigned long)ptr;
		bigbook_insert(lptr, len);
		DEBUG("big allocator claimed %d pages\n", len/4096);
		return ptr;
	}
	else
	{
		// for now
		ptr = little_alloc(len);
		return ptr;
	}
}

void yfree ( void* ptr )
{
	unsigned long lptr = (unsigned long)ptr;
	if (lptr % BIG_PAGE_THRESHOLD)
	{
		// small granularity
		little_free(ptr); // for now
	}
	else
	{
		// big granularity
		unsigned long len = bigbook_eatlen(lptr);
		if (len == 0)
		{
			// we must have been mistaken and it was a little block
			little_free(ptr);
			return;
		}
		page_deallocate(ptr, len);
		DEBUG("big allocator released %d pages\n", len/4096);
	}
}

#endif
