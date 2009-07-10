CC=clang
#CFLAGS=-O0 -gfull -pipe
CFLAGS=-arch x86_64 -O4 -DNDEBUG -pipe
LDFLAGS=-arch x86_64 -pipe

test: test.o pages.o sched_multilevel.o sched_roundrobin.o task.o test.o
	$(CC) $(LDFLAGS) -o $@ $^

test.o: test.c config.h task.h yarns.h
	$(CC) $(CFLAGS) -c -o $@ $<

pages.o: pages.c pages.h
	$(CC) $(CFLAGS) -c -o $@ $<

sched_multilevel.o: sched_multilevel.c config.h scheduler.h lock.h
	$(CC) $(CFLAGS) -c -o $@ $<

sched_roundrobin.o: sched_roundrobin.c config.h scheduler.h lock.h
	$(CC) $(CFLAGS) -c -o $@ $<

task.o: task.c task.h scheduler.h pages.h lock.h config.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o test