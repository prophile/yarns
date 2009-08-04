#ifndef __YARNS_YARN__
#define __YARNS_YARN__

#include "config.h"

typedef unsigned long yarn_t;

#define YARNS_SCHEDULER_ROUND_ROBIN 1
#define YARNS_SCHEDULER_ROTATING_STAIRCASE 2
#define YARNS_SCHEDULER_COMPLETELY_FAIR 3

yarn_t yarn_current ( void ) __attribute__((pure));
yarn_t yarn_new ( void (*routine)(void*), void* udata, int nice );
void yarn_yield ( yarn_t target );
void yarn_process ( unsigned long otherThreadCount, int primaryScheduler, int secondaryScheduler );
void yarn_mark ();
const char* yarns_version ();

unsigned long yarn_next_token ();
void yarn_suspend_on_tokens ( const unsigned long* token, unsigned long ntok );
inline static void yarn_suspend_on_token ( unsigned long token )
	{ yarn_suspend_on_tokens(&token, 1); }
void yarn_suspend_on_time ( unsigned long usecs );
void yarn_signal_token ( unsigned long token );

#endif
