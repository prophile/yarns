#include "scheduler.h"
#include "alloc.h"
#include "queue.h"
#include "config.h"
#include "debug.h"
#include <assert.h>
#include <stdbool.h>

#define DEBUG_MODULE DEBUG_SCHEDULER

#define LAYERS 20

typedef struct _staircase_block
{
	unsigned long timeslices[LAYERS];
	fifo* queues[LAYERS];
} staircase_block;

typedef struct _job_entry
{
	unsigned long timeslice;
	unsigned long lastTimeslice;
	unsigned long pid;
	unsigned long originalLevel;
	unsigned long lastLevel;
} job_entry;

typedef struct _scheduler_staircase scheduler_staircase;

struct _scheduler_staircase
{
	staircase_block* active;
	staircase_block* exhausted;
};

static void scheduler_staircase_deallocate ( scheduler* mst )
{
	scheduler_staircase* sched = (scheduler_staircase*)mst->context;
	yfree(sched->active);
	yfree(sched->exhausted);
	yfree(sched);
	yfree(mst);
}

static void scheduler_staircase_schedule ( scheduler* mst, unsigned long pid, scheduler_priority prio )
{
	scheduler_staircase* sched = (scheduler_staircase*)mst->context;
	int p = (int)prio;
	job_entry* je = yalloc(sizeof(job_entry));
	if (p > 0) p--;
	p *= 2;
	je->timeslice = YARNS_TIMESLICE;
	je->lastTimeslice = 0;
	je->pid = pid;
	je->originalLevel = p;
	je->lastLevel = p;
	fifo_enqueue_pointer(sched->active->queues[p], je);
}

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MIN3(x,y,z) MIN(x,MIN(y,z))

static void scheduler_staircase_reschedule ( scheduler* mst, scheduler_job* job )
{
	scheduler_staircase* sched = (scheduler_staircase*)mst->context;
	job_entry* je;
	if (job->pid != 0)
		je = (job_entry*)job->data;
	else
		je = 0;
	assert(je);
	sched->active->timeslices[je->lastLevel] -= (je->lastTimeslice - job->runtime);
	je->timeslice -= (je->lastTimeslice - job->runtime);
	if (je->timeslice == 0)
	{
		je->lastLevel = je->originalLevel;
		je->timeslice = YARNS_TIMESLICE;
		fifo_enqueue_pointer(sched->exhausted->queues[je->originalLevel], je);
	}
	else
	{
		unsigned long nextLevel;
		if (je->lastLevel != LAYERS-1)
			nextLevel = ++(je->lastLevel);
		else
			nextLevel = je->lastLevel;
		fifo_enqueue_pointer(sched->exhausted->queues[nextLevel], je);
	}
}

static void scheduler_staircase_unschedule ( scheduler* mst, scheduler_job* job )
{
	scheduler_staircase* sched = (scheduler_staircase*)mst->context;
	job_entry* je;
	if (job->pid != 0)
		je = (job_entry*)job->data;
	else
		je = 0;
	assert(je);
	sched->active->timeslices[je->lastLevel] -= je->lastTimeslice;
	yfree(je);
}

static void scheduler_staircase_select ( scheduler* mst, scheduler_job* job )
{
	scheduler_staircase* sched = (scheduler_staircase*)mst->context;
	unsigned long selectedLevel = 0;
	job_entry* je;
	while (selectedLevel < LAYERS)
	{
		if (sched->active->timeslices[selectedLevel] == 0)
		{
			selectedLevel++;
			continue;
		}
		if (fifo_length(sched->active->queues[selectedLevel]) == 0)
		{
			selectedLevel++;
			continue;
		}
		break;
	}
	if (selectedLevel == LAYERS) // expired
	{
		if (job->data == 1) // this means we did a swap
		{
			job->pid = 0;
			job->runtime = 0;
			job->data = 0;
		}
		else
		{
			staircase_block* block = sched->active;
			sched->active = sched->exhausted;
			sched->exhausted = block;
			DEBUG("did a swap\n");
			job->data = 1;
			scheduler_staircase_select(mst, job); // now tail-recursive!
		}
	}
	else
	{
		je = fifo_dequeue_pointer(sched->active->queues[selectedLevel]);
		assert (je);
		job->pid = je->pid;
		je->lastTimeslice = MIN3(je->timeslice, sched->active->timeslices[selectedLevel], YARNS_TIMESLICE);
		job->runtime = je->lastTimeslice;
		job->data = (unsigned long)je;
	}
}

scheduler* sched_allocate_staircase ()
{
	int i;
	scheduler* mst = yalloc(sizeof(scheduler));
	scheduler_staircase* sched = yalloc(sizeof(scheduler_staircase));
	sched->active = yalloc(sizeof(staircase_block));
	sched->exhausted = yalloc(sizeof(staircase_block));
	for (i = 0; i < LAYERS; i++)
	{
		sched->active->queues[i] = fifo_new();
		sched->active->timeslices[i] = YARNS_TIMESLICE * 5;
		sched->exhausted->queues[i] = fifo_new();
		sched->exhausted->timeslices[i] = YARNS_TIMESLICE * 5;
	}
	assert(sizeof(unsigned long) == sizeof(void*));
	mst->context = sched;
	mst->deallocate = scheduler_staircase_deallocate;
	mst->schedule = scheduler_staircase_schedule;
	mst->select = scheduler_staircase_select;
	mst->unschedule = scheduler_staircase_unschedule;
	mst->reschedule = scheduler_staircase_reschedule;
	return mst;
}
