#include "yarn.h"
#include "config.h"
#include "context.h"
#include <stdlib.h>
#include "master_scheduler.h"
#include <assert.h>
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
#include <stdbool.h>
#include "preempt.h"
#include "timer.h"
#include "yarn-internal.h"
#include "pool.h"
#include <string.h>
#include "wait_graph.h"
#include "rbtree.h"

#define DEBUG_MODULE DEBUG_YARNS

#define STACK_SIZE (YARNS_STACK_PAGES*4096)

#define UNSCHEDULE ~0UL

typedef struct _yarn yarn;

static bool live = 0;

struct _yarn
{
	yarn_context_t context;
	yarn_t pid;
	int nice;
	void* stackBase;
	bool isSuspended;
	unsigned long suspensionCore;
	scheduler_job suspensionJob;
};

#define PROCESSES_PER_BLOCK 64

typedef struct _process_block
{
	yarn* block[PROCESSES_PER_BLOCK];
} process_block;

static rbtree* process_block_tree;
static int maxpid = 1;
static pthread_t threads[32];
static wait_graph* wg;
static unsigned long coreCount;
static unsigned long wgProcResponsibility = 0;

static yarn** _ptable ( yarn_t pid )
{
	unsigned long major, minor;
	process_block* block;
	if (!process_block_tree)
		process_block_tree = rbtree_new();
	major = pid / PROCESSES_PER_BLOCK;
	minor = pid % PROCESSES_PER_BLOCK;
	if (!rbtree_search(process_block_tree, major, (void**)&block))
	{
		block = yalloc(sizeof(process_block));
		rbtree_insert(process_block_tree, major, block);
	}
	return &(block->block[minor]);
}

#define PTABLE(i) (*_ptable(i))

typedef struct _yarn_thread_data
{
	yarn_context_t sched_context;
	yarn* yarn_current;
	yarns_time_t start_time;
	unsigned long runtime;
	yarn_t next;
	bool shouldSuspend;
} yarn_thread_data;
#ifdef YARNS_ENABLE_SMP
pthread_key_t _ttd;
#define TTDGET() yarn_thread_data* _ttd_local = (yarn_thread_data*)pthread_getspecific(_ttd)
#define TTD (*_ttd_local)
static void TTDINIT_MAIN ()
{
	int rc;
	rc = pthread_key_create(&_ttd, 0);
	assert(rc == 0);
}

static pool* yarn_pool = 0;

static pool* get_yarn_pool ()
{
	if (!yarn_pool)
	{
		yarn_pool = pool_create(sizeof(yarn), YARNS_MAX_PROCESSES);
	}
	return yarn_pool;
}

static void TTDINIT()
{
	yarn_thread_data* ttd = (yarn_thread_data*)yalloc(sizeof(yarn_thread_data));
	ttd->yarn_current = NULL;
	ttd->runtime = 0;
	pthread_setspecific(_ttd, ttd);
}
#else
static yarn_thread_data _ttd;
#define TTDGET()
#define TTD _ttd
#define TTDINIT_MAIN() {}
#define TTDINIT() { _ttd.yarn_current = 0; _ttd.runtime = 0; }
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

static void* allocate_stack ( yarn_context_t* ctx )
{
	void* ptr = page_allocate(STACK_SIZE, 0, PAGE_READ | PAGE_WRITE);
	yarn_context_set_stack(ctx, ptr, STACK_SIZE);
	yarn_check_stack(ptr);
	DEBUG("allocated a stack at %p\n", ptr);
	return ptr;
}

static void deallocate_stack ( void* stackbase )
{
	unsigned char* sb = (unsigned char*)stackbase;
	DEBUG("deallocated a stack at %p\n", sb);
	page_deallocate(sb, STACK_SIZE);
}

volatile int __make_trap = 1; // this traps the odd situation where yarn_context_make doesn't work and we get random code jumping in here

static yarn_t list_insert ( yarn* active_yarn, int nice )
{
	yarn_t pid = maxpid++;
	DEBUG("pid(%d)=yarn(%p)\n", pid, active_yarn);
	assert(maxpid < YARNS_MAX_PROCESSES);
	PTABLE(pid) = active_yarn;
	active_yarn->pid = pid;
	active_yarn->nice = nice;
	return pid;
}

typedef struct _yarn_launch_data
{
	yarn* active_yarn;
	void (*routine)(void*);
	void* udata;
} yarn_launch_data;

static void yarn_launcher ( yarn_launch_data* ld )
{
	TTDGET();
	yarn* active_yarn = ld->active_yarn;
	void (*routine)(void*) = ld->routine;
	void* udata = ld->udata;
	// basically a bootstrap routine for each yarn which runs the routine then cleans up
	yfree(ld);
	routine(udata);
	DEBUG("yarn %p completed, yarn_context_set\n", active_yarn);
	TTD.runtime = UNSCHEDULE;
	PTABLE(active_yarn->pid) = 0;
	yarn_context_set(&(TTD.sched_context));
}

static void prepare_context ( yarn* active_yarn, void (*routine)(void*), void* udata )
{
	yarn_launch_data* ld = yalloc(sizeof(yarn_launch_data));
	yarn_context_t* uctx = &active_yarn->context;
	ld->active_yarn = active_yarn;
	ld->routine = routine;
	ld->udata = udata;
	DEBUG("running yarn_context_make on ctx: %p\n", uctx);
	//yarn_context_make(uctx, basic_launch, 0);
	yarn_context_make(uctx, (void (*)(void*))yarn_launcher, ld);
}

static void make_trap_abort ()
{
	raise(SIGABRT);
}

static void init_context ( yarn_context_t* uctx )
{
	__make_trap = 0;
	yarn_context_init(uctx);
	if (__make_trap != 0)
		make_trap_abort ();
	__make_trap = 1;
}

yarn_t yarn_current ( void )
{
	TTDGET();
	return TTD.yarn_current->pid;
}

static scheduler_priority prio_lookup ( int nice )
{
	if (nice > 20) nice = 20;
	if (nice < -20) nice = -20;
	switch (nice)
	{
		case -20: return SCHED_PRIO_TITANIC;
		case -19: return SCHED_PRIO_VERY_HIGH;
		case -18: return SCHED_PRIO_VERY_HIGH;
		case -17: return SCHED_PRIO_VERY_HIGH;
		case -16: return SCHED_PRIO_VERY_HIGH;
		case -15: return SCHED_PRIO_VERY_HIGH;
		case -14: return SCHED_PRIO_VERY_HIGH;
		case -13: return SCHED_PRIO_VERY_HIGH;
		case -12: return SCHED_PRIO_VERY_HIGH;
		case -11: return SCHED_PRIO_HIGH;
		case -10: return SCHED_PRIO_HIGH;
		case -9:  return SCHED_PRIO_HIGH;
		case -8:  return SCHED_PRIO_HIGH;
		case -7:  return SCHED_PRIO_HIGH;
		case -6:  return SCHED_PRIO_HIGH;
		case -5:  return SCHED_PRIO_HIGH;
		case -4:  return SCHED_PRIO_HIGH;
		case -3:  return SCHED_PRIO_NORMAL;
		case -2:  return SCHED_PRIO_NORMAL;
		case -1:  return SCHED_PRIO_NORMAL;
		case 0:   return SCHED_PRIO_NORMAL;
		case 1:   return SCHED_PRIO_NORMAL;
		case 2:   return SCHED_PRIO_NORMAL;
		case 3:   return SCHED_PRIO_NORMAL;
		case 4:   return SCHED_PRIO_LOW;
		case 5:   return SCHED_PRIO_LOW;
		case 6:   return SCHED_PRIO_LOW;
		case 7:   return SCHED_PRIO_LOW;
		case 8:   return SCHED_PRIO_LOW;
		case 9:   return SCHED_PRIO_LOW;
		case 10:  return SCHED_PRIO_LOW;
		case 11:  return SCHED_PRIO_LOW;
		case 12:  return SCHED_PRIO_LOW;
		case 13:  return SCHED_PRIO_LOW;
		case 14:  return SCHED_PRIO_LOW;
		case 15:  return SCHED_PRIO_VERY_LOW;
		case 16:  return SCHED_PRIO_VERY_LOW;
		case 17:  return SCHED_PRIO_VERY_LOW;
		case 18:  return SCHED_PRIO_VERY_LOW;
		case 19:  return SCHED_PRIO_VERY_LOW;
		case 20:  return SCHED_PRIO_VERY_LOW;
	}
}

yarn_t yarn_new ( void (*routine)(void*), void* udata, int nice )
{
	// grab memory
	yarn* active_yarn;
	yarn_t pid;
	//prepare_context(0);
	pool* p = get_yarn_pool();
	active_yarn = (yarn*)pool_allocate(p);
	assert(active_yarn);
	// prepare the context
	init_context(&active_yarn->context);
	// set up stack and such
	active_yarn->stackBase = allocate_stack(&(active_yarn->context));
	// run yarn_context_make to direct it over to the bootstrap
	prepare_context(active_yarn, routine, udata);
	// insert it into the yarn list
	pid = list_insert(active_yarn, nice);
	if (live)
		master_sched_schedule(pid, prio_lookup(nice));
	return pid;
}

static void yarn_switch ( unsigned long runtime, yarn_t target )
{
	TTDGET();
	TTD.runtime = runtime;
	TTD.next = target;
	DEBUG("yarn_context_swap in switch (rt=%lu, n=%lu)\n", runtime, target);
	yarn_context_swap(&(TTD.yarn_current->context), &(TTD.sched_context));
}

void yarn_yield ( yarn_t target )
{
	// ignore target for now
	TTDGET();
	yarns_time_t t = yarns_time();
	yarns_time_t dt = TTD.start_time - t;
	yarn_switch((unsigned long)(TTD.runtime - dt), target);
}

#if YARNS_SYNERGY == YARNS_SYNERGY_PREEMPTIVE
static void yarn_preempt_handle ( unsigned long thread )
{
	pthread_kill(threads[thread], SIGUSR2);
}

static void preempt_signal_handler ( int sig, struct __siginfo* info, yarn_context_t* context )
{
	assert(sig == SIGUSR2);
	TTDGET();
	memcpy(TTD.yarn_current->context.uc_mcontext, context->uc_mcontext, context->uc_mcsize);
	TTD.runtime = 0;
	yarn_context_set(&(TTD.sched_context));
}
#endif

static void yarn_processor ( unsigned long procID )
{
	// this is the main routine run by each thread
	struct sigaction sa;
	void* stackRoot;
	volatile int breakProcessor = 0;
	int rc;
	unsigned deadSleepTime = YARNS_DEAD_SLEEP_TIME;
	// init the thread yarn data
	TTDINIT();
	TTDGET();
	// make sure it initted properly
	assert(&TTD);
	// repeatedly ask the scheduler for the next job
	scheduler_job activeJob;
#ifdef BASE_CONTEXT_NEEDS_STACK
	stackRoot = allocate_stack(&(TTD.sched_context));
#endif
#if YARNS_SYNERGY == YARNS_SYNERGY_PREEMPTIVE
	sa.sa_sigaction = (void (*)(int, struct __siginfo *, void *))preempt_signal_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGUSR2);
	sigaction(SIGUSR2, &sa, 0);
#endif
	activeJob.pid = 0;
	activeJob.runtime = 0;
	activeJob.data = 0;
	activeJob.next = 0;
	activeJob.priority = SCHED_PRIO_NORMAL;
	while (!breakProcessor)
	{
		TTD.shouldSuspend = 0;
		if (procID == wgProcResponsibility)
		{
			wait_graph_lock(wg);
			wait_graph_time_process(wg);
			wait_graph_unlock(wg);
			wgProcResponsibility++;
			wgProcResponsibility %= coreCount;
		}
		master_sched_select(procID, &activeJob);
		if (activeJob.pid == 0)
		{
			usleep(deadSleepTime);
			deadSleepTime += (deadSleepTime / 2);
			continue;
		}
		DEBUG("job %lu @ proc %d (rt: %lu us)\n", activeJob.pid, (int)procID, activeJob.runtime);
		if (activeJob.pid >= maxpid)
		{
			DEBUG("got pid %lu which is out-of-bound!\n", activeJob.pid);
			usleep(deadSleepTime);
			continue;
		}
		if (!PTABLE(activeJob.pid))
		{
			DEBUG("got dead pid %lu\n", activeJob.pid);
			usleep(deadSleepTime);
			continue;
		}
		// set all the stuff up
		TTD.yarn_current = PTABLE(activeJob.pid);
		TTD.start_time = yarns_time();
		TTD.next = 0;
		TTD.runtime = activeJob.runtime;
		// swap contexts
		DEBUG("yarn_context_swap to yarn: %p\n", TTD.yarn_current);
#if YARNS_SYNERGY == YARNS_SYNERGY_PREEMPTIVE
		preempt(yarns_time() + activeJob.runtime, procID);
		preempt_enable();
#endif
		rc = yarn_context_swap(&(TTD.sched_context), &(TTD.yarn_current->context));
#if YARNS_SYNERGY == YARNS_SYNERGY_PREEMPTIVE
		preempt_disable();
#endif
		activeJob.next = TTD.next;
		// check it actually worked
		if (rc == 0)
			perror("yarn_context_swap failed");
		// set the runtime
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
		activeJob.runtime = MIN(TTD.runtime, activeJob.runtime);
		// if we're unscheduling, yfree up memory
		if (TTD.runtime == UNSCHEDULE)
		{
			master_sched_unschedule(procID, &activeJob);
			deallocate_stack(TTD.yarn_current->stackBase);
			pool_free(get_yarn_pool(), TTD.yarn_current);
		}
		else if (TTD.shouldSuspend)
		{
			TTD.yarn_current->isSuspended = 1;
			TTD.yarn_current->suspensionCore = procID;
			memcpy(&(TTD.yarn_current->suspensionJob), &activeJob, sizeof(activeJob));
		}
		else
		{
			master_sched_reschedule(procID, &activeJob);
		}
		deadSleepTime = YARNS_DEAD_SLEEP_TIME;
	}
#ifdef BASE_CONTEXT_NEEDS_STACK
	deallocate_stack(stackRoot);
#endif
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

void yarn_resume ( yarn_t yarn )
{
	if (PTABLE(yarn)->isSuspended)
	{
		PTABLE(yarn)->isSuspended = 0;
		master_sched_reschedule(PTABLE(yarn)->suspensionCore, &(PTABLE(yarn)->suspensionJob));
	}
}

void yarn_process ( unsigned long otherThreadCount, int primaryScheduler, int secondaryScheduler )
{
// look through yarns
	unsigned long nprocs;
	int i;
#ifdef YARNS_ENABLE_SMP
	nprocs = numprocs() - otherThreadCount;
	if (nprocs < 1) nprocs = 1;
	master_sched_init(nprocs, primaryScheduler, secondaryScheduler);
#else
	master_sched_init(1, primaryScheduler, secondaryScheduler);
#endif
#if YARNS_SYNERGY == YARNS_SYNERGY_PREEMPTIVE
	preempt_init();
	preempt_handle = yarn_preempt_handle;
#endif
	TTDINIT_MAIN();
	wg = wait_graph_new();
	// set up all the yarns in the scheduler
	for (i = 1; i < maxpid; i++)
	{
		master_sched_schedule(i, prio_lookup(PTABLE(i)->nice));
	}
	live = 1;
	DEBUG("starting with context API: %s\n", YARN_CONTEXT_API);
#ifdef YARNS_ENABLE_SMP
		// create the secondary threads
	for (i = 1; i < nprocs; i++)
	{
#if YARNS_SYNERGY == YARNS_SYNERGY_PREEMPTIVE
		preempt_disable();
#endif
		pthread_create(&threads[i], NULL, (void* (*)(void*))yarn_processor, (void*)i);
	}
#endif
	threads[i] = pthread_self();
	coreCount = nprocs;
	// run the primary thread
	yarn_processor(0);
}

#if YARNS_SYNERGY == YARNS_SYNERGY_MARKED
void yarn_mark ()
{
	TTDGET();
	yarns_time_t t = yarns_time();
	yarns_time_t dt = TTD.start_time - t;
	if (dt > TTD.runtime)
	{
		yarn_switch(0, 0);
	}
}
#else
void yarn_mark ()
{
}
#endif

const char* yarns_version ()
{
#ifdef YARNS_CURRENT_VERSION
	return YARNS_CURRENT_VERSION;
#else
	return "?";
#endif
}

unsigned long yarn_next_token ()
{
	unsigned long tok;
	wait_graph_lock(wg);
	tok = wait_graph_next_token(wg);
	wait_graph_unlock(wg);
	return tok;
}

static void suspend ()
{
	TTDGET();
	TTD.shouldSuspend = 1;
	yarns_time_t t = yarns_time();
	yarns_time_t dt = TTD.start_time - t;
	DEBUG("suspend() called\n");
	if (dt > TTD.runtime)
		yarn_switch(0, 0);
	else
		yarn_switch(dt, 0);
}

void yarn_suspend_on_tokens ( const unsigned long* tokens, unsigned long ntok )
{
	yarn_t currentYarn;
	unsigned long i;
	TTDGET();
	currentYarn = TTD.yarn_current->pid;
	wait_graph_lock(wg);
	wait_graph_insert(wg, currentYarn);
	for (i = 0; i < ntok; i++)
	{
		wait_graph_insert_token(wg, currentYarn, tokens[i]);
	}
	wait_graph_unlock(wg);
	suspend();
}

void yarn_suspend_on_time ( unsigned long usecs )
{
	yarn_t currentYarn;
	unsigned long i;
	TTDGET();
	currentYarn = TTD.yarn_current->pid;
	wait_graph_lock(wg);
	wait_graph_insert(wg, currentYarn);
	wait_graph_insert_time(wg, currentYarn, yarns_time() + usecs);
	wait_graph_unlock(wg);
	suspend();
}

void yarn_signal_token ( unsigned long token )
{
	DEBUG("signal: %lu\n", token);
	wait_graph_lock(wg);
	wait_graph_signal(wg, token);
	wait_graph_unlock(wg);
}
