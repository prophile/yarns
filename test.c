#include "task.h"
#include <stdio.h>

void myRoutine1 ( void* inText )
{
	printf("Print: %s\n", (const char*)inText);
}

int main ()
{
	task_new ( myRoutine1, (void*)"Hello, world!" );
	task_new ( myRoutine1, (void*)"This is routine 2" );
	task_new ( myRoutine1, (void*)"This is routine 3" );
	task_process ();
	return 0;
}
