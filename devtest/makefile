ifndef RUNDIR
    $(error You must specify RUNDIR)
endif

CFLAGS = -g -DSUN -lssl -fpermissive
CFLAGS += -Wno-write-strings
CC = g++
CCF = $(CC) $(CFLAGS)

all: test

test: test.o $(RUNDIR)/libclientReplFs.a
	$(CCF) -o test test.o -L$(RUNDIR) -lclientReplFs

test.o: test.c
	$(CCF) -I$(RUNDIR) -c test.c

clean:
	rm -f test *.o *.a
