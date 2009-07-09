#include "task.h"
#define _XOPEN_SOURCE
#include <ucontext.h>
#include <stdlib.h>
#include "scheduler.h"
#include <assert.h>
#include "config.h"
#include <stdio.h>

typedef struct _task_meta
{
	ucontext_t context;
	unsigned long scheduler_data;
} task_meta;

#define STACK_SIZE ((YARNS_STACK_PAGES*4096)-sizeof(task_meta)-sizeof(void*))

typedef struct _task
{
	task_meta meta;
	task_t next;
	task_t prev;
	unsigned char stack[STACK_SIZE];
} task;

#ifdef YARNS_ENABLE_SMP
#define TLV __thread
#else
#define TLV
#endif

static task* task_list = 0;
static TLV ucontext_t sched_context;
static TLV task* task_current = 0;
static TLV unsigned long runtime_remaining = 0;

static void task_launcher ( task* tsk, void (*routine)(void*), void* udata )
{
	routine(udata);
	runtime_remaining = SCHEDULER_UNSCHEDULE;
}

task_t task_new ( void (*routine)(void*), void* udata )
{
	task* tsk;
	tsk = (task*)valloc(sizeof(task));
	assert(tsk);
	tsk->meta.scheduler_data = 0;
	getcontext(&tsk->meta.context);
	if (task_list)
	{
		if (task_list->prev)
		{
			tsk->prev = task_list->prev;
			tsk->next = task_list;
			tsk->prev->next = tsk;
			task_list->prev = tsk;
		}
		else
		{
			task_list->prev = task_list->next = tsk;
			tsk->prev = tsk->next = task_list;
		}
	}
	else
	{
		tsk->next = 0;
		tsk->prev = 0;
	}
	task_list = tsk;
	tsk->meta.context.uc_stack.ss_size = STACK_SIZE;
	tsk->meta.context.uc_stack.ss_sp = &(tsk->stack[0]);
	tsk->meta.context.uc_stack.ss_flags = 0;
	makecontext(&(tsk->meta.context), (void (*)(void))task_launcher, 3, tsk, routine, udata);
	return (task_t)tsk;
}

void task_yield ( task_t target )
{
	// ignore target for now
	runtime_remaining = 1;
	swapcontext(&(task_current->meta.context), &sched_context);
}

static void task_processor ( unsigned long procID )
{
	volatile int breakProcessor = 0;
	int rc;
	scheduler_job activeJob;
	activeJob.pid = 0;
	activeJob.runtime = 0;
	activeJob.processor = procID;
	activeJob.data = 0;
	while (!breakProcessor)
	{
		scheduler_select(&activeJob);
		task_current = (task*)activeJob.pid;
		task_current->meta.context.uc_link = &sched_context;
		rc = swapcontext(&sched_context, &(task_current->meta.context));
		if (rc == -1)
			perror("swapcontext failed");
		activeJob.runtime = runtime_remaining;
		if (runtime_remaining == SCHEDULER_UNSCHEDULE)
		{
			task_current->prev->next = task_current->next;
			task_current->next->prev = task_current->prev;
			free((void*)task_current);
		}
	}
}

void task_process ()
{
	task* tsk = task_list;
	if (!tsk) return;
#ifdef YARNS_ENABLE_SMP
	#error SMP not yet supported
#else
	scheduler_init(1);
#endif
	do
	{
		scheduler_insert((unsigned long)tsk);
		tsk = tsk->next;
	} while (tsk && tsk != task_list);
#ifdef YARNS_ENABLE_SMP
#else
	task_processor(0);
#endif
}
