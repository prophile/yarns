#include "preempt.h"
#include <sys/time.h>
#include <signal.h>
#include "rbtree.h"
#include "lock.h"
#include "alloc.h"
#include <stdio.h>
#include "atomic.h"

static volatile unsigned long preempt_disable_count = 0;
preempt_handler preempt_handle = 0;

static void preempt_signal ( int sig );

void preempt_disable ()
{
	atomic_add(&preempt_disable_count, 1);
}

void preempt_enable ()
{
	atomic_add(&preempt_disable_count, -1);
}

static unsigned long preempt_param;
static lock_t preempt_lock;

static rbtree* preempt_schedule;

void preempt_init ( void )
{
	preempt_schedule = rbtree_new();
	lock_init(&preempt_lock);
}

unsigned long preempt_time ( void )
{
	unsigned long t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = (tv.tv_usec / 1000) + (tv.tv_sec * 1000);
	return t;
}

static inline void convert ( struct timeval* tv, unsigned long t )
{
	tv->tv_sec  = t / 1000;
	tv->tv_usec = (t * 1000) % 1000000;
}

static void reconfigure_timer ( void )
{
	unsigned long t, timestamp;
	struct itimerval value;
	convert(&value.it_interval, 0);
	if (rbtree_size(preempt_schedule) > 0)
	{
		timestamp = rbtree_min(preempt_schedule);
		t = preempt_time();
		if (timestamp < t)
			t = 0;
		else
			t = timestamp - t;
		printf("preempting in %lu microseconds for target time = %lu (current time = %lu)\n", t, timestamp, preempt_time());
		convert(&value.it_value, t);
		rbtree_search(preempt_schedule, timestamp, (void**)&preempt_param);
		signal(SIGVTALRM, preempt_signal);
		setitimer(ITIMER_VIRTUAL, &value, 0);
		rbtree_remove(preempt_schedule, t);
	}
	else
	{
		convert(&value.it_value, 0);
		setitimer(ITIMER_VIRTUAL, &value, 0);
	}
}

static void preempt_signal ( int sig )
{
	printf("got preempt signal\n");
	bool lockOK = lock_trylock(&preempt_lock);
	if (lockOK && preempt_handle && !preempt_disable_count)
	{
		unsigned long min = rbtree_min(preempt_schedule);
		unsigned long param;
		rbtree_search(preempt_schedule, min, (void**)&param);
		reconfigure_timer();
		lock_unlock(&preempt_lock);
		preempt_handle(param);
	}
	else
	{
		// either we're locked or we've been told not to preempt, wait 5ms
		struct itimerval value;
		convert(&value.it_interval, 0);
		convert(&value.it_value, 5);
		setitimer(ITIMER_VIRTUAL, &value, 0);
		if (lockOK)
			lock_unlock(&preempt_lock);
	}
}

void preempt ( unsigned long timestamp, unsigned long param )
{
	printf("preempt(%lu, %lu)\n", timestamp, param);
	lock_lock(&preempt_lock);
	rbtree_insert(preempt_schedule, timestamp, (void*)param);
	reconfigure_timer();
	lock_unlock(&preempt_lock);
}

void preempt_cancel ( unsigned long timestamp )
{
	lock_lock(&preempt_lock);
	rbtree_remove(preempt_schedule, timestamp);
	reconfigure_timer();
	lock_unlock(&preempt_lock);
}
