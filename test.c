#include "yarn.h"
#include <stdio.h>
#include "alloc.h"

void myRoutine1 ( void* inText )
{
	printf("Print: %s\n", (const char*)inText);
}

int main ( int argc, char** argv )
{
	yallocinit();
	yarn_new ( myRoutine1, (void*)"Hello, world!" );
	yarn_new ( myRoutine1, (void*)"This is routine 2" );
	yarn_new ( myRoutine1, (void*)"This is routine 3" );
	yarn_process ();
	return 0;
}
