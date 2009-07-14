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
#if YARNS_SELECTED_TARGET == YARNS_TARGET_MACH
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#define STACK_SIZE (YARNS_STACK_PAGES*4096)

typedef struct _yarn
{
	yarn_t next;
	yarn_t prev;
	ucontext_t context;
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
static void TTDINIT_MAIN ()
{
	int rc;
	rc = pthread_key_create(&_ttd, 0);
	assert(rc == 0);
}

static void TTDINIT()
{
	yarn_thread_data* ttd = (yarn_thread_data*)malloc(sizeof(yarn_thread_data));
	ttd->yarn_current = NULL;
	ttd->runtime_remaining = 0;
	pthread_setspecific(_ttd, ttd);
}
#else
static yarn_thread_data _ttd;
#define TTD _ttd
#define TTDINIT_MAIN() {}
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

static void prepare_context ( yarn* active_yarn, void (*routine)(void*), void* udata )
{
	ucontext_t* uctx = &active_yarn->context;
	//printf("running makecontext on ctx: %p\n", uctx);
	//makecontext(uctx, basic_launch, 0);
	makecontext(uctx, (void (*)())yarn_launcher, 3, active_yarn, routine, udata);
}

static void fetch_context ( ucontext_t* uctx )
{
	__make_trap = 0;
	getcontext(uctx);
	assert(__make_trap == 0);
	__make_trap = 1;
}

yarn_t yarn_new ( void (*routine)(void*), void* udata )
{
	// grab memory
	yarn* active_yarn;
	//prepare_context(0);
	active_yarn = (yarn*)malloc(sizeof(yarn));
	assert(active_yarn);
	// prepare the context
	fetch_context(&active_yarn->context);
	// set up stack and such
	allocate_stack(&(active_yarn->context));
	// run makecontext to direct it over to the bootstrap
	prepare_context(active_yarn, routine, udata);
	// insert it into the yarn list
	list_insert(active_yarn);
	return (yarn_t)active_yarn;
}

void yarn_yield ( yarn_t target )
{
	// ignore target for now
	TTD.runtime_remaining = 1;
	// pop back over to the scheduler
	swapcontext(&(TTD.yarn_current->context), &(TTD.sched_context));
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
		TTD.yarn_current->context.uc_link = &(TTD.sched_context);
		// swap contexts
		rc = swapcontext(&(TTD.sched_context), &(TTD.yarn_current->context));
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
			deallocate_stack(&(TTD.yarn_current->context));
			free(TTD.yarn_current);
		}
	}
	deallocate_stack(&(TTD.sched_context));
}

// number of parallel processing cores, for the moment assume 2
#ifdef YARNS_ENABLE_SMP
static unsigned long numprocs ()
{
#if YARNS_SELECTED_TARGET == YARNS_TARGET_MACH
	unsigned long nproc = 2;
	sysctlbyname("hw.availcpu", &nproc, NULL, NULL, 0);
	printf("numprocs: %lu\n", nproc);
	return nproc;
#else
	return 2;
#endif
}
#endif

void yarn_process ()
{
// look through yarns
	unsigned long nprocs;
	yarn* active_yarn = yarn_list;
	if (!active_yarn) return;
#ifdef YARNS_ENABLE_SMP
	nprocs = numprocs();
	smp_sched_init(nprocs);
#else
	smp_sched_init(1);
#endif
	TTDINIT_MAIN();
	// set up all the yarns in the scheduler
	do
	{
		smp_sched_insert((unsigned long)active_yarn);
		active_yarn = active_yarn->next;
	} while (active_yarn && active_yarn != yarn_list);
#ifdef YARNS_ENABLE_SMP
	{
		// create the secondary threads
		int i;
		for (i = 1; i < nprocs; i++)
		{
			pthread_t pt;
			pthread_create(&pt, NULL, (void* (*)(void*))yarn_processor, (void*)i);
		}
	}
#endif
	// run the primary thread
	yarn_processor(0);
}
