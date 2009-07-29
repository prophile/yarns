#include "scheduler.h"
#include "alloc.h"
#include "queue.h"
#include "config.h"
#include <assert.h>
#include <stdbool.h>

#if YARNS_SCHEDULER == YARNS_SCHED_STAIRCASE

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

struct _scheduler
{
	staircase_block* active;
	staircase_block* exhausted;
	bool inSwap;
};

scheduler* scheduler_init ()
{
	int i;
	scheduler* sched = yalloc(sizeof(scheduler));
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
	return sched;
}

void scheduler_free ( scheduler* sched )
{
	yfree(sched->active);
	yfree(sched->exhausted);
	yfree(sched);
}

void scheduler_insert ( scheduler* sched, unsigned long pid, scheduler_priority prio )
{
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

void scheduler_select ( scheduler* sched, scheduler_job* job )
{
	job_entry* je;
	unsigned long selectedLevel;
	if (job->pid != 0)
		je = (job_entry*)job->data;
	else
		je = 0;
	if (!sched->inSwap)
	{
		if (job->runtime == SCHEDULER_UNSCHEDULE)
		{
			sched->active->timeslices[je->lastLevel] -= je->lastTimeslice;
			yfree(je);
		}
		else if (je)
		{
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
	}
	if (job->next == SCHEDULER_WANT_IDLE)
	{
		job->pid = 0;
		job->runtime = 0;
		job->data = 0;
		return;
	}
	selectedLevel = 0;
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
		if (sched->inSwap)
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
			sched->inSwap = 1;
			scheduler_select(sched, job);
			sched->inSwap = 0;
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

#endif
