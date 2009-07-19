#ifndef __YARNS_PREEMPT__
#define __YARNS_PREEMPT__

#include <stdbool.h>

typedef void (*preempt_handler)(void);

unsigned long preempt_time ();
void preempt ( unsigned long timestamp );

extern preempt_handler preempt_handle;
extern volatile bool preempt_disable;

#endif
