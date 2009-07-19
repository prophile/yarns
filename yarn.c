#include "yarn.h"
#define _XOPEN_SOURCE
#include <ucontext.h>
#include <stdlib.h>
#include "smp_scheduler.h"
#include <assert.h>
#include "config.h"
#include <stdio.h>
#include "pages.h"
#include <signal.h>
#include <pthread.h>
#if YARNS_SELECTED_TARGET == YARNS_TARGET_MACH
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#include <unistd.h>
#include "alloc.h"
#include "debug.h"

#define STACK_GROWTH_DOWN

#define DEBUG_MODULE DEBUG_YARNS

#define STACK_SIZE (YARNS_STACK_PAGES*4096)

typedef struct _yarn yarn;

struct _yarn
{
	ucontext_t context;
	unsigned long pid;
};

static yarn* process_table[YARNS_MAX_PROCESSES] = { 0 };
static int maxpid = 1;

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
	yarn_thread_data* ttd = (yarn_thread_data*)yalloc(sizeof(yarn_thread_data));
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
		DEBUG("ul&4095=%d\n", (int)(ul % 4096));
		assert(0);
	}
}

static void yarn_launcher ( yarn* active_yarn, void (*routine)(void*), void* udata )
{
	// basically a bootstrap routine for each yarn which runs the routine then cleans up
	routine(udata);
	DEBUG("yarn %p completed, setcontext\n", active_yarn);
	TTD.runtime_remaining = SCHEDULER_UNSCHEDULE;
	process_table[active_yarn->pid] = 0;
	setcontext(&(TTD.sched_context));
}

static void allocate_stack ( ucontext_t* ctx )
{
	ctx->uc_stack.ss_size = STACK_SIZE;
	ctx->uc_stack.ss_sp = page_allocate(STACK_SIZE, 0, PAGE_READ | PAGE_WRITE);
	assert(ctx->uc_stack.ss_sp);
	ctx->uc_stack.ss_flags = 0;
	yarn_check_stack(ctx->uc_stack.ss_sp);
}

static void deallocate_stack ( ucontext_t* ctx )
{
	page_deallocate(ctx->uc_stack.ss_sp, STACK_SIZE);
}

int __make_trap = 1; // this traps the odd situation where makecontext doesn't work and we get random code jumping in here

static yarn_t list_insert ( yarn* active_yarn )
{
	yarn_t pid = maxpid++;
	DEBUG("pid(%d)=yarn(%p)\n", pid, active_yarn);
	assert(maxpid < YARNS_MAX_PROCESSES);
	process_table[pid] = active_yarn;
	active_yarn->pid = pid;
	return pid;
}

static void prepare_context ( yarn* active_yarn, void (*routine)(void*), void* udata )
{
	ucontext_t* uctx = &active_yarn->context;
	uctx->uc_link = 0;
	DEBUG("running makecontext on ctx: %p\n", uctx);
	//makecontext(uctx, basic_launch, 0);
	makecontext(uctx, (void (*)())yarn_launcher, 3, active_yarn, routine, udata);
}

static void make_trap_abort ()
{
	raise(SIGABRT);
}

static void fetch_context ( ucontext_t* uctx )
{
	__make_trap = 0;
	getcontext(uctx);
	if (__make_trap != 0)
		make_trap_abort ();
	__make_trap = 1;
}

yarn_t yarn_current ( void )
{
	return TTD.yarn_current->pid;
}

yarn_t yarn_new ( void (*routine)(void*), void* udata )
{
	// grab memory
	yarn* active_yarn;
	yarn_t pid;
	//prepare_context(0);
	active_yarn = (yarn*)yalloc(sizeof(yarn));
	assert(active_yarn);
	// prepare the context
	fetch_context(&active_yarn->context);
	// set up stack and such
	allocate_stack(&(active_yarn->context));
	// run makecontext to direct it over to the bootstrap
	prepare_context(active_yarn, routine, udata);
	// insert it into the yarn list
	pid = list_insert(active_yarn);
	return pid;
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
	unsigned deadSleepTime = YARNS_DEAD_SLEEP_TIME;
	// init the thread yarn data
	TTDINIT();
	// make sure it initted properly
	assert(&TTD);
	// repeatedly ask the scheduler for the next job
	scheduler_job activeJob;
#ifdef BASE_CONTEXT_NEEDS_STACK
	allocate_stack(&(TTD.sched_context));
#endif
	TTD.sched_context.uc_link = 0;
	activeJob.pid = 0;
	activeJob.runtime = 0;
	activeJob.data = 0;
	while (!breakProcessor)
	{
		smp_sched_select(procID, &activeJob);
		if (activeJob.pid == 0)
		{
			usleep(deadSleepTime);
			deadSleepTime *= 2;
			continue;
		}
		DEBUG("job %lu @ proc %d\n", activeJob.pid, (int)procID);
		assert(activeJob.pid < maxpid);
		assert(process_table[activeJob.pid]);
		// set all the stuff up
		TTD.yarn_current = process_table[activeJob.pid];
		// swap contexts
		rc = swapcontext(&(TTD.sched_context), &(TTD.yarn_current->context));
		// check it actually worked
		if (rc == -1)
			perror("swapcontext failed");
		// set the runtime
		activeJob.runtime = TTD.runtime_remaining;
		// if we're unscheduling, yfree up memory
		if (TTD.runtime_remaining == SCHEDULER_UNSCHEDULE)
		{
			deallocate_stack(&(TTD.yarn_current->context));
			yfree(TTD.yarn_current);
		}
		deadSleepTime = YARNS_DEAD_SLEEP_TIME;
	}
	deallocate_stack(&(TTD.sched_context));
}

// number of parallel processing cores, for the moment assume 2
#ifdef YARNS_ENABLE_SMP
static unsigned long numprocs ()
{
#if defined(YARNS_OVERRIDE_CORE_COUNT)
	return YARNS_OVERRIDE_CORE_COUNT;
#elif YARNS_SELECTED_TARGET == YARNS_TARGET_MACH
	unsigned long nproc = 4;
	size_t oldLen = sizeof(nproc);
	int namevec[2] = { CTL_HW, HW_NCPU };
	int rc = sysctl(namevec, 2, &nproc, &oldLen, NULL, 0);
	if (rc)
	{
		perror("sysctl failed, assuming 2 procs");
		return 2;
	}
	else
	{
		DEBUG("numprocs: %lu\n", nproc);
		return nproc;
	}
#else
	return 2;
#endif
}
#endif

void yarn_process ()
{
// look through yarns
	unsigned long nprocs;
	int i;
#ifdef YARNS_ENABLE_SMP
	nprocs = numprocs();
	smp_sched_init(nprocs);
#else
	smp_sched_init(1);
#endif
	TTDINIT_MAIN();
	// set up all the yarns in the scheduler
	for (i = 1; i < maxpid; i++)
	{
		smp_sched_insert(i);
	}
#ifdef YARNS_ENABLE_SMP
		// create the secondary threads
	for (i = 1; i < nprocs; i++)
	{
		pthread_t pt;
		pthread_create(&pt, NULL, (void* (*)(void*))yarn_processor, (void*)i);
	}
#endif
	// run the primary thread
	yarn_processor(0);
}
