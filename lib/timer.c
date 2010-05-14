#include "timer.h"
#include <stdbool.h>
#include <sys/time.h>
#include <stdlib.h>

static bool initted = 0;
static yarns_time_t initialTime = 0;

yarns_time_t yarns_time ( void )
{
	yarns_time_t t;
	struct timeval tv;
	if (!initted)
	{
		initted = 1;
		initialTime = yarns_time();
	}
	gettimeofday(&tv, NULL);
	t = (tv.tv_usec / 1000) + (tv.tv_sec * 1000);
	return t - initialTime;
}
