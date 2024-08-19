CC=gcc
CPPFLAGS=-lrt -lpthread -I.
DEPS = operations.h
OBJS := shm_ipc.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CPPFLAGS)

ops: $(OBJS)
	$(CC) -o $@ $^ $(CPPFLAGS)

run: ops
	./ops

clean:
	rm -f pidpr *.o