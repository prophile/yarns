#include "yarn.h"
#define _XOPEN_SOURCE
#include <ucontext.h>
#include <stdlib.h>
#include "scheduler.h"
#include <assert.h>
#include "config.h"
#include <stdio.h>
#include "pages.h"
#include <pthread.h>

typedef struct _yarn_meta
{
	ucontext_t context;
	//unsigned long scheduler_data;
} yarn_meta;

#define STACK_SIZE (YARNS_STACK_PAGES*4096)

typedef struct _yarn
{
	unsigned char* stack;
	yarn_meta meta;
	yarn_t next;
	yarn_t prev;
} yarn;

static yarn* yarn_list = 0;
typedef struct _yarn_thread_data
{
	ucontext_t sched_context;
	yarn* yarn_current;
	unsigned long runtime_remaining;
} yarn_thread_data;
#ifdef YARNS_ENABLE_SMP
pthread_key_t _ttd;
#define TTD (*(yarn_thread_data*)(pthread_getspecific(_ttd)))
#define TTDINIT() { int rc; yarn_thread_data* ttd = (yarn_thread_data*)malloc(sizeof(yarn_thread_data)); ttd->yarn_current = NULL; ttd->runtime_remaining = 0; rc = pthread_key_create(&_ttd, 0); assert(rc == 0); }
#else
static yarn_thread_data _ttd;
#define TTD _ttd
#define TTDINIT() { _ttd.yarn_current = 0; _ttd.runtime_remaining = 0; }
#endif

static void yarn_check_stack ( const void* base )
{
	// make sure that base is page-aligned (ie a multiple of 4096)
	unsigned long ul = (unsigned long)base;
	if (ul & 4095)
	{
		printf("ul&4095=%d\n", ul & 4095);
		assert(0);
	}
}

static void yarn_launcher ( yarn* tsk, void (*routine)(void*), void* udata )
{
	// basically a bootstrap routine for each yarn which runs the routine then cleans up
	routine(udata);
	TTD.runtime_remaining = SCHEDULER_UNSCHEDULE;
}

static volatile int make_trap = 1; // this traps the odd situation where makecontext doesn't work and we get random code jumping in here

yarn_t yarn_new ( void (*routine)(void*), void* udata )
{
	// grab memory
	yarn* tsk;
	make_trap = 0;
	tsk = (yarn*)malloc(sizeof(yarn));
	tsk->stack = page_allocate(STACK_SIZE);
	assert(tsk);
	//tsk->meta.scheduler_data = 0;
	// prepare the context
	getcontext(&tsk->meta.context);
	assert(make_trap == 0);
	// insert it into the yarn list
	if (yarn_list)
	{
		if (yarn_list->prev)
		{
			tsk->prev = yarn_list->prev;
			tsk->next = yarn_list;
			tsk->prev->next = tsk;
			yarn_list->prev = tsk;
		}
		else
		{
			yarn_list->prev = yarn_list->next = tsk;
			tsk->prev = tsk->next = yarn_list;
		}
	}
	else
	{
		tsk->next = 0;
		tsk->prev = 0;
	}
	yarn_list = tsk;
	// set up stack and such
	tsk->meta.context.uc_stack.ss_size = STACK_SIZE;
	tsk->meta.context.uc_stack.ss_sp = &(tsk->stack[0]);
	tsk->meta.context.uc_stack.ss_flags = 0;
	yarn_check_stack(tsk->meta.context.uc_stack.ss_sp);
	// run makecontext to direct it over to the bootstrap
	makecontext(&(tsk->meta.context), (void (*)())yarn_launcher, 3, tsk, routine, udata);
	make_trap = 1;
	return (yarn_t)tsk;
}

void yarn_yield ( yarn_t target )
{
	// ignore target for now
	TTD.runtime_remaining = 1;
	// pop back over to the scheduler
	swapcontext(&(TTD.yarn_current->meta.context), &(TTD.sched_context));
}

static void yarn_processor ( unsigned long procID )
{
	// this is the main routine run by each thread
	volatile int breakProcessor = 0;
	int rc;
	// init the thread yarn data
	TTDINIT();
	// make sure it initted properly
	assert(&TTD);
	// repeatedly ask the scheduler for the next job
	scheduler_job activeJob;
	activeJob.pid = 0;
	activeJob.runtime = 0;
	activeJob.processor = procID;
	activeJob.data = 0;
	while (!breakProcessor)
	{
		scheduler_select(&activeJob);
		printf("job %lu @ proc %d\n", activeJob.pid, procID);
		// set all the stuff up
		TTD.yarn_current = (yarn*)activeJob.pid;
		TTD.yarn_current->meta.context.uc_link = &(TTD.sched_context);
		// swap contexts
		rc = swapcontext(&(TTD.sched_context), &(TTD.yarn_current->meta.context));
		// check it actually worked
		if (rc == -1)
			perror("swapcontext failed");
		// set the runtime
		activeJob.runtime = TTD.runtime_remaining;
		// if we're unscheduling, free up memory
		if (TTD.runtime_remaining == SCHEDULER_UNSCHEDULE)
		{
			TTD.yarn_current->prev->next = TTD.yarn_current->next;
			TTD.yarn_current->next->prev = TTD.yarn_current->prev;
			page_deallocate(TTD.yarn_current->stack, STACK_SIZE);
			free(TTD.yarn_current);
		}
	}
}

// number of parallel processing cores, for the moment assume 2
static int numprocs ()
{
	return 2;
}

void yarn_process ()
{
// look through yarns
	yarn* tsk = yarn_list;
	if (!tsk) return;
#ifdef YARNS_ENABLE_SMP
	scheduler_init(numprocs());
#else
	scheduler_init(1);
#endif
	// set up all the yarns in the scheduler
	do
	{
		scheduler_insert((unsigned long)tsk);
		tsk = tsk->next;
	} while (tsk && tsk != yarn_list);
#ifdef YARNS_ENABLE_SMP
	{
		// create the secondary threads
		int i, n = numprocs();
		for (i = 1; i < n; i++)
		{
			pthread_t pt;
			pthread_create(&pt, NULL, (void* (*)(void*))yarn_processor, (void*)i);
		}
	}
#endif
	// run the primary thread
	yarn_processor(0);
}
