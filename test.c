#include "yarn.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "alloc.h"

static void myRoutine1 ( void* param )
{
	printf("This is routine: %lu\n", (unsigned long)param);
}

static void yarnify ( unsigned long n )
{
	yarn_new ( myRoutine1, (void*)n );
}

static void quit ( void* p )
{
	exit(0);
}

int main ( int argc, char** argv )
{
	int i;
	srand(time(NULL));
	yallocinit();
	//stress_test_big_allocator();
	for (i = 0; i < 30000; i++)
	{
		yarnify(i);
	}
	yarn_new(quit, 0);
	yarn_process ();
	
	return 0;
}
