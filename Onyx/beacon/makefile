
all:	../bld/obj/beacon ../bld/obj/probe

../bld/obj/beacon:	beacon.c
	gcc -o ../bld/obj/beacon beacon.c -ggdb -Wall -Werror

../bld/obj/probe:	probe.c
	gcc -o ../bld/obj/probe probe.c -ggdb -Wall -Werror

../bld/obj/clean:
	rm ../bld/obj/beacon ../bld/obj/probe
