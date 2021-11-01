OBJS	= dmf2mod.o core.o modules.o mod.o dmf.o
SOURCE	= dmf2mod.cpp core.cpp modules.cpp mod.cpp dmf.cpp
HEADER	= core.h modules.h mod.h dmf.h zconf.h zlib.h

ifeq ($(OS),Windows_NT)
OUT	= dmf2mod.exe
ZLIB_MAKE	= $(MAKE) --directory=zlib --makefile=win32/Makefile.gcc
ZLIB_CLEAN	= $(MAKE) clean --directory=zlib --makefile=win32/Makefile.gcc
else
OUT	= dmf2mod
ZLIB_MAKE	= cd zlib && chmod +x ./configure && ./configure --static && $(MAKE) && cd ..
ZLIB_CLEAN	= cd zlib && $(MAKE) clean && cd ..
endif

CC	 = g++
FLAGS	 = -std=c++17 -Izlib -Izstr -g -c -Wall -Wno-unknown-pragmas
LFLAGS	 = -lm zlib/libz.a

all: $(OBJS) libz.a
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

dmf2mod.o: dmf2mod.cpp
	$(CC) $(FLAGS) dmf2mod.cpp

core.o: core.cpp
	$(CC) $(FLAGS) core.cpp

modules.o: modules.cpp
	$(CC) $(FLAGS) modules.cpp

mod.o: mod.cpp
	$(CC) $(FLAGS) mod.cpp

dmf.o: dmf.cpp libz.a
	$(CC) $(FLAGS) dmf.cpp

zlib libz.a:
	$(ZLIB_MAKE)

.PHONY: clean zlibclean zlib

clean:
	$(RM) $(OUT) $(OBJS)

zlibclean:
	$(ZLIB_CLEAN)
