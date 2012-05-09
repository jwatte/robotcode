
all:	../bld/snarf ../bld/mandel

../bld/snarf:	snarf.cpp
	g++ -O0 -g -o ../bld/snarf snarf.cpp -lserial

../bld/mandel:	mandel.cpp
	g++ -O0 -g -o ../bld/mandel mandel.cpp
	../bld/mandel ../bld/mandel.tga
	mtpaint ../bld/mandel.tga
