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

int main ( int argc, char** argv )
{
	int i;
	srand(time(NULL));
	yallocinit();
	//stress_test_big_allocator();
	for (i = 0; i < 10000; i++)
	{
		yarnify(i);
	}
	yarn_process ();
	
	return 0;
}
