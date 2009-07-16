#include "alloc.h"
#include "pages.h"
#include <stdlib.h>

typedef struct _bigalloc_book bigalloc_book;

struct _bigalloc_book
{
	unsigned long lptr;
	unsigned long len;
	bigalloc_book* next;
};

#define BIG_BOOK_HASH_SIZE 256

static bigalloc_book books[BIG_BOOK_HASH_SIZE];

void yallocinit ()
{
	int i;
	for (i = 0; i < BIG_BOOK_HASH_SIZE; i++)
	{
		books[i].lptr = 0;
		books[i].len = 0;
		books[i].next = 0;
	}
}

static void bigbook_insert ( unsigned long lptr, unsigned long len )
{
	unsigned idx = lptr;
	idx /= 4096;
	idx %= BIG_BOOK_HASH_SIZE;
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
}

static unsigned long bigbook_eatlen ( unsigned long lptr )
{
	unsigned idx = lptr;
	bigalloc_book* book;
	idx /= 4096;
	idx %= BIG_BOOK_HASH_SIZE;
	book = &books[idx];
	while (book && book->lptr != lptr)
		book = book->next;
	if (book->lptr == lptr)
	{
		unsigned long len = book->len;
		if (book != &books[idx])
		{
			// it was allocated, eat it
			bigalloc_book* secondBook = &books[idx];
			while (secondBook->next != book)
				secondBook = secondBook->next;
			secondBook->next = book->next;
			yfree(book);
		}
		return len;
	}
	else
	{
		return 0; // didn't find it... strange
	}
}

void* yalloc ( unsigned long len )
{
	void* ptr;
	unsigned long lptr;
	if (len > 2048)
	{
		len += 4095;
		len &= ~4095;
		ptr = page_allocate(len);
		lptr = (unsigned long)ptr;
		bigbook_insert(lptr, len);
	}
	else
	{
		// for now
		return malloc(len);
	}
}

void yfree ( void* ptr )
{
	unsigned long lptr = (unsigned long)ptr;
	if (lptr & 4095)
	{
		// small granularity
		free(ptr); // for now
	}
	else
	{
		// big granularity
		unsigned long len = bigbook_eatlen(lptr);
		page_deallocate(ptr, len);
	}
}
