CC	= gcc
CFLAGS	= -g -pthread

default: all

all: dtlb

dtlb: dtlb.c
	$(CC) $(CFLAGS) -o dtlb_latency dtlb_latency.c
	$(CC) $(CFLAGS) -o stlb_latency stlb_latency.c
	$(CC) $(CFLAGS) -o itlb itlb.c
	$(CC) $(CFLAGS) -o spy spy.c
	$(CC) $(CFLAGS) -o victim victim.c
	$(CC) $(CFLAGS) -o dtlb dtlb.c

clean: 
	rm -f dtlb
