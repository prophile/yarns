#include "preempt.h"
#include <sys/time.h>
#include <signal.h>

volatile bool preempt_disable = 1;
preempt_handler preempt_handle = 0;

unsigned long preempt_time ()
{
	unsigned long t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = tv.tv_usec + (tv.tv_sec * 1000000);
}

static void preempt_signal ( int sig )
{
	preempt_handle();
}

void preempt ( unsigned long timestamp )
{
	unsigned long t;
	struct itimerval value;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	t = preempt_time() - timestamp;
	value.it_value.tv_sec = t / 1000000;
	value.it_value.tv_usec = t % 1000000;
	signal(SIGVTALRM, preempt_signal);
	setitimer(ITIMER_VIRTUAL, &value, 0);
}
