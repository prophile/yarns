#include "yarn.h"
#include <stdio.h>

void myRoutine1 ( void* inText )
{
	printf("Print: %s\n", (const char*)inText);
}

int main ()
{
	yarn_new ( myRoutine1, (void*)"Hello, world!" );
	yarn_new ( myRoutine1, (void*)"This is routine 2" );
	yarn_new ( myRoutine1, (void*)"This is routine 3" );
	yarn_process ();
	return 0;
}
