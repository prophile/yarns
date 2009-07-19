#include "yarn.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "alloc.h"

void myRoutine1 ( void* inText )
{
	printf("Print: %s\n", (const char*)inText);
}

static void test_allocator ( unsigned long len )
{
	void* ptr = yalloc(len);
	memset(ptr, 0, len);
	yfree(ptr);
}

static void stress_test_big_allocator ()
{
	unsigned long i, ntests = 10000;
	printf("Stress testing big allocator.\n");
	for (i = 0; i < ntests; i++)
	{
		unsigned long len = 2048 + (rand() % 30000);
		test_allocator(len);
	}
	printf("Done!\n");
}

static void stress_test_little_allocator ()
{
	unsigned long i, ntests = 10000;
	printf("Stress testing little allocator.\n");
	for (i = 0; i < ntests; i++)
	{
		unsigned long len = (rand() % 2048);
		test_allocator(len);
	}
	printf("Done!\n");
}

int main ( int argc, char** argv )
{
	srand(time(NULL));
	yallocinit();
	//stress_test_big_allocator();
	yarn_new ( myRoutine1, (void*)"Hello, world!" );
	yarn_new ( myRoutine1, (void*)"This is routine 2" );
	yarn_new ( myRoutine1, (void*)"This is routine 3" );
	yarn_process ();
	
	return 0;
}
