#include "yarn.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "alloc.h"
#include "sync.h"

yarn_lock_t yl;

static void myRoutine1 ( void* param )
{
	printf("Enter: %lu\n", (unsigned long)param);
	yarn_yield(0);
	printf("Exit: %lu\n", (unsigned long)param);
}

static void myRoutine2 ( void* param )
{
	printf("Long-running operation, lol?\n");
	yarn_lock_lock(&yl);
	sleep(5);
	yarn_lock_unlock(&yl);
	printf("Finished eet!\n");
}

static void yarnify ( unsigned long n )
{
	yarn_new ( myRoutine1, (void*)n );
}

static void quit ( void* p )
{
	printf("Entered quit operation!\n");
	yarn_lock_lock(&yl);
	exit(0);
	yarn_lock_unlock(&yl);
}

int main ( int argc, char** argv )
{
	int i;
	srand(time(NULL));
	yarn_lock_init(&yl);
	//stress_test_big_allocator();
	for (i = 0; i < 5; i++)
	{
		if (i == 3)
			yarn_new(myRoutine2, 0);
		yarnify(i);
	}
	//yarn_new(quit, 0);
	yarn_process ();
	
	return 0;
}
