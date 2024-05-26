CC=gcc
STD=_GNU_SOURCE

TARGETS=net

.c.o:
	$(CC) -c $(CFLAGS) -D$(STD) $(WARN) $<

all:	$(TARGETS)

net: net.c
	$(CC) net.c -o net -g3

clean:
	rm -f *.o

clobber: clean
	rm -f $(TARGETS)