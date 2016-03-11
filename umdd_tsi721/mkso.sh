#!/bin/sh

g++ -O3 -Wall -ggdb -I./inc -I../common/include \
	-I../fabric_management/librio_switch/inc \
	-DNODEBUG -std=gnu++0x -pthread  \
	-DUSER_MODE_DRIVER -Wall -Wno-format -Wno-sign-compare -Wno-write-strings -DRIO_ANY_ADDR=0 \
	-ggdb -O0  \
	src/time_utils.c src/mapfile.cc src/mport.cc src/dmachan.cc src/crc32.c src/psem.cc src/pshm.cc \
	-fPIC   -shared -Wl,-soname,libUMDd.so.1 -o libUMDd.so.1.0 \
	-lrt -L../common/libs_so -lmport
