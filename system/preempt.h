#ifndef __YARNS_PREEMPT__
#define __YARNS_PREEMPT__

#include <stdbool.h>
#include "config.h"

#if YARNS_SYNERGY == YARNS_SYNERGY_PREEMPTIVE

typedef void (*preempt_handler)(unsigned long);

void preempt_init ( void );
void preempt ( unsigned long timestamp, unsigned long parameter );
void preempt_cancel ( unsigned long timestamp );

void preempt_disable ();
void preempt_enable ();

extern preempt_handler preempt_handle;

#endif

#endif
