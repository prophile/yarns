#include "lock.h"
#include "scheduler.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef YARNS_ENABLE_SMP
static _Bool ismultiproc;
static volatile lock_t schedlock;
#endif

static void panic ( const char* msg )
{
	printf("Panic: %s\n", msg);
	_Exit(1);
}

typedef struct _sched_queue_job sched_queue_job;

struct _sched_queue_job
{
	unsigned long pid;
	unsigned long skipper_index;
	sched_queue_job* next;
};

#define TIMESLICE 10000
#define LAYERS 4

extern void* malloc ( unsigned long bytes );
extern void free ( void* ptr );

static sched_queue_job* layer_heads[LAYERS];
static sched_queue_job* layer_tails[LAYERS];

static unsigned long skipper_index = 1;

static void lock ()
{
#ifdef YARNS_ENABLE_SMP
	if (!ismultiproc)
		return;
	lock_lock(&schedlock);
#endif
}

static void unlock ()
{
#ifdef YARNS_ENABLE_SMP
	if (!ismultiproc)
		return;
	lock_unlock(&schedlock);
#endif
}

void scheduler_init ( unsigned long procs )
{
	int i;
#ifdef YARNS_ENABLE_SMP
	ismultiproc = (procs > 1) ? 1 : 0;
#else
	if (procs > 1)
		panic("no smp support in sched");
#endif
	for (i = 0; i < LAYERS; i++)
	{
		layer_heads[i] = 0;
		layer_tails[i] = 0;
	}
#ifdef YARNS_ENABLE_SMP
	if (ismultiproc)
		lock_init(&schedlock);
#endif
}

static void insert ( unsigned long pid, int skip, int level )
{
	if (!layer_heads[level])
	{
		layer_heads[level] = layer_tails[level] = malloc(sizeof(sched_queue_job));
		layer_heads[level]->pid = pid;
		layer_heads[level]->next = 0;
		layer_heads[level]->skipper_index = skip ? skipper_index : 0;
	}
	else
	{
		if (!layer_tails[level])
			panic("bad layer tail");
		layer_tails[level]->next = malloc(sizeof(sched_queue_job));
		layer_tails[level] = layer_tails[level]->next;
		layer_tails[level]->pid = pid;
		layer_tails[level]->next = 0;
		layer_tails[level]->skipper_index = skip ? skipper_index : 0;
	}
}

void scheduler_insert ( unsigned long pid )
{
	lock();
	insert(pid, 0, 0);
	unlock();
}

static void trueselect ( scheduler_job* job, int secondary )
{
	int i, found = 0;
	if (job->pid > 0)
	{
		if (job->runtime == 0)
		{
			insert(job->pid, 0, (job->data == LAYERS - 1) ? job->data : (job->data + 1));
		}
		else
		{
			insert(job->pid, 1, job->data);
		}
	}
	for (i = 0; i < LAYERS; i++)
	{
		if (!layer_heads[i])
			continue;
		sched_queue_job* qjob = layer_heads[i];
		layer_heads[i] = layer_heads[i]->next;
		if (layer_heads[i] == 0)
			layer_tails[i] = 0;
		if (qjob->skipper_index == skipper_index)
			continue;
		job->pid = qjob->pid;
		job->data = i;
		job->runtime = TIMESLICE;
		free(qjob);
		found = 1;
		break;
	}
	if (!found)
	{
		if (secondary)
		{
			job->pid = 0;
			job->runtime = 0;
			job->data = 0;
		}
		else
		{
			skipper_index++;
			trueselect(job, 1);
		}
	}
}

void scheduler_select ( scheduler_job* job )
{
	lock();
	trueselect(job, 0);
	unlock();
}
