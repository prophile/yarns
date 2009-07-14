#include "yarn.h"
#define _XOPEN_SOURCE
#include <ucontext.h>
#include <stdlib.h>
#include "smp_scheduler.h"
#include <assert.h>
#include "config.h"
#include <stdio.h>
#include "pages.h"
#include <pthread.h>

typedef struct _yarn_meta
{
	ucontext_t context;
	//unsigned long smp_sched_data;
} yarn_meta;

#define STACK_SIZE (YARNS_STACK_PAGES*4096)

typedef struct _yarn
{
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

#if YARNS_SELECTED_TARGET == YARNS_TARGET_MACH
#define BASE_CONTEXT_NEEDS_STACK
#endif

static void yarn_check_stack ( const void* base )
{
	// make sure that base is page-aligned (ie a multiple of 4096)
	unsigned long ul = (unsigned long)base;
	if (ul & 4095 || !ul)
	{
		printf("ul&4095=%d\n", (int)(ul % 4096));
		assert(0);
	}
}

static void yarn_launcher ( yarn* active_yarn, void (*routine)(void*), void* udata )
{
	// basically a bootstrap routine for each yarn which runs the routine then cleans up
	routine(udata);
	TTD.runtime_remaining = SCHEDULER_UNSCHEDULE;
}

static void allocate_stack ( ucontext_t* ctx )
{
	ctx->uc_stack.ss_size = STACK_SIZE;
	ctx->uc_stack.ss_sp = page_allocate(STACK_SIZE);
	assert(ctx->uc_stack.ss_sp);
	ctx->uc_stack.ss_flags = 0;
	yarn_check_stack(ctx->uc_stack.ss_sp);
}

static void deallocate_stack ( ucontext_t* ctx )
{
	page_deallocate(ctx->uc_stack.ss_sp, STACK_SIZE);
}

static void basic_launch ()
{
	puts("basic_launch");
	_Exit(0);
}

int __make_trap = 1; // this traps the odd situation where makecontext doesn't work and we get random code jumping in here

static void list_insert ( yarn* active_yarn )
{
	if (yarn_list)
	{
		if (yarn_list->prev)
		{
			active_yarn->prev = yarn_list->prev;
			active_yarn->next = yarn_list;
			active_yarn->prev->next = active_yarn;
			yarn_list->prev = active_yarn;
		}
		else
		{
			yarn_list->prev = yarn_list->next = active_yarn;
			active_yarn->prev = active_yarn->next = yarn_list;
		}
	}
	else
	{
		active_yarn->next = 0;
		active_yarn->prev = 0;
	}
	yarn_list = active_yarn;
}

yarn_t yarn_new ( void (*routine)(void*), void* udata )
{
	// grab memory
	yarn* active_yarn;
	active_yarn = (yarn*)malloc(sizeof(yarn));
	assert(active_yarn);
	// set up stack and such
	allocate_stack(&(active_yarn->meta.context));
	// prepare the context
	__make_trap = 0;
	getcontext(&active_yarn->meta.context);
	assert(__make_trap == 0);
	__make_trap = 1;
	// insert it into the yarn list
	list_insert(active_yarn);
	// run getcontext & makecontext to direct it over to the bootstrap
	//makecontext(&(active_yarn->meta.context), (void (*)())yarn_launcher, 3, active_yarn, routine, udata);
	makecontext(&(active_yarn->meta.context), basic_launch, 0);
	return (yarn_t)active_yarn;
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
#ifdef BASE_CONTEXT_NEEDS_STACK
	allocate_stack(&(TTD.sched_context));
#endif
	activeJob.pid = 0;
	activeJob.runtime = 0;
	activeJob.data = 0;
	while (!breakProcessor)
	{
		smp_sched_select(procID, &activeJob);
		printf("job %lu @ proc %d\n", activeJob.pid, (int)procID);
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
			deallocate_stack(&(TTD.yarn_current->meta.context));
			free(TTD.yarn_current);
		}
	}
	deallocate_stack(&(TTD.sched_context));
}

// number of parallel processing cores, for the moment assume 2
#ifdef YARNS_ENABLE_SMP
static int numprocs ()
{
	return 2;
}
#endif

void yarn_process ()
{
// look through yarns
	yarn* active_yarn = yarn_list;
	if (!active_yarn) return;
#ifdef YARNS_ENABLE_SMP
	smp_sched_init(numprocs());
#else
	smp_sched_init(1);
#endif
	// set up all the yarns in the scheduler
	do
	{
		smp_sched_insert((unsigned long)active_yarn);
		active_yarn = active_yarn->next;
	} while (active_yarn && active_yarn != yarn_list);
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
