CC=clang
ARCH=i386
CFLAGS=-O0 -gfull -pipe -Wall -arch $(ARCH)
#CFLAGS=-O4 -DNDEBUG -pipe -arch $(ARCH)
LDFLAGS=-L. -arch $(ARCH)

test: test.o libyarns.dylib
	$(CC) $(LDFLAGS) -lyarns -o $@ $<
	
libyarns.dylib: pages.o sched_multilevel.o sched_roundrobin.o yarn.o smp_scheduler.o sched_random.o alloc.o rbtree.o
	$(LD) -lc -dylib -exported_symbols_list exports.txt $(LDFLAGS) -o $@ $^ /usr/lib/dylib1.o

test.o: test.c config.h yarn.h yarns.h
	$(CC) $(CFLAGS) -c -o $@ $<

pages.o: pages.c pages.h debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

sched_multilevel.o: sched_multilevel.c config.h scheduler.h debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

sched_roundrobin.o: sched_roundrobin.c config.h scheduler.h debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

sched_random.o: sched_random.c config.h scheduler.h debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

yarn.o: yarn.c yarn.h scheduler.h pages.h lock.h config.h debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

smp_scheduler.o: smp_scheduler.c smp_scheduler.h scheduler.h config.h lock.h debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

alloc.o: alloc.c alloc.h pages.h debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

rbtree.o: rbtree.c rbtree.h alloc.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o test libyarns.dylib