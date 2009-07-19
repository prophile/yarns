#ifndef __YARNS_YARN__
#define __YARNS_YARN__

typedef unsigned long yarn_t;

yarn_t yarn_current ( void ) __attribute__((pure));
yarn_t yarn_new ( void (*routine)(void*), void* udata );
void yarn_yield ( yarn_t target );
void yarn_process ();

#endif
