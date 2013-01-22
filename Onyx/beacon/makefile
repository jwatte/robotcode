
CFLAGS:=-std=gnu11 -ggdb -Wall -Werror
CC:=gcc
LFLAGS:=-lm -ggdb

all:	../bld/obj/beacon ../bld/obj/probe

../bld/obj/%:	%.c
	$(CC) -o $@ $(CFLAGS) $(LFLAGS) $<

clean:
	rm ../bld/obj/beacon ../bld/obj/probe
