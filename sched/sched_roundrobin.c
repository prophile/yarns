#include "scheduler.h"
#include <stdbool.h>
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "alloc.h"
#include "debug.h"
#include "queue.h"

#define DEBUG_MODULE DEBUG_SCHEDULER

// basically cycles through jobs, a very nice and simple scheduler

static void scheduler_round_robin_deallocate ( scheduler* sched )
{
	fifo_free((fifo*)sched->context);
	yfree(sched);
}

static void scheduler_round_robin_schedule ( scheduler* sched, unsigned long pid, scheduler_priority prio )
{
	fifo* q = (fifo*)sched->context;
	fifo_enqueue(q, pid);
}

static void scheduler_round_robin_select ( scheduler* sched, scheduler_job* job )
{
	fifo* q = (fifo*)sched->context;
	if (fifo_length(q) == 0)
	{
		job->pid = 0;
		job->runtime = 0;
	}
	else
	{
		job->pid = fifo_dequeue(q);
		job->runtime = YARNS_TIMESLICE;
	}
}

static void scheduler_round_robin_reschedule ( scheduler* sched, scheduler_job* job )
{
	fifo* q = (fifo*)sched->context;
	fifo_enqueue(q, job->pid);
}

static void scheduler_round_robin_unschedule ( scheduler* sched, scheduler_job* job )
{
}

scheduler* sched_allocate_round_robin ()
{
	scheduler* mst = yalloc(sizeof(scheduler));
	mst->context = fifo_new();
	mst->deallocate = scheduler_round_robin_deallocate;
	mst->schedule = scheduler_round_robin_schedule;
	mst->unschedule = scheduler_round_robin_unschedule;
	mst->reschedule = scheduler_round_robin_reschedule;
	mst->select = scheduler_round_robin_select;
	return mst;
}
