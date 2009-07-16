CC=clang
CFLAGS=-O0 -gfull -pipe -Wall
#CFLAGS=-O4 -DNDEBUG -pipe
LDFLAGS=-pipe

test: test.o pages.o sched_multilevel.o sched_roundrobin.o yarn.o smp_scheduler.o sched_random.o alloc.o
	$(CC) $(LDFLAGS) -o $@ $^

test.o: test.c config.h yarn.h yarns.h
	$(CC) $(CFLAGS) -c -o $@ $<

pages.o: pages.c pages.h
	$(CC) $(CFLAGS) -c -o $@ $<

sched_multilevel.o: sched_multilevel.c config.h scheduler.h
	$(CC) $(CFLAGS) -c -o $@ $<

sched_roundrobin.o: sched_roundrobin.c config.h scheduler.h
	$(CC) $(CFLAGS) -c -o $@ $<

sched_random.o: sched_random.c config.h scheduler.h
	$(CC) $(CFLAGS) -c -o $@ $<

yarn.o: yarn.c yarn.h scheduler.h pages.h lock.h config.h
	$(CC) $(CFLAGS) -c -o $@ $<

smp_scheduler.o: smp_scheduler.c smp_scheduler.h scheduler.h config.h lock.h
	$(CC) $(CFLAGS) -c -o $@ $<

alloc.o: alloc.c alloc.h pages.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o test