OBJS	= dmf2mod.o converter.o modules.o mod.o dmf.o
SOURCE	= dmf2mod.cpp converter.cpp modules.cpp mod.cpp dmf.cpp
HEADER	= converter.h modules.h mod.h dmf.h zconf.h zlib.h

ifeq ($(OS),Windows_NT)
OUT	= dmf2mod.exe
ZLIB_MAKE	= make --directory=zlib --makefile=win32/Makefile.gcc
ZLIB_CLEAN	= make clean --directory=zlib --makefile=win32/Makefile.gcc
else
OUT	= dmf2mod
ZLIB_MAKE	= ./zlib/configure --static && make --directory=zlib
ZLIB_CLEAN	= make clean --directory=zlib
endif

CC	 = g++
FLAGS	 = -std=c++17 -Izlib -Izstr -g -c -Wall -Wno-unknown-pragmas
LFLAGS	 = -lm zlib/libz.a

all: $(OBJS) zlib/libz.a
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

dmf2mod.o: dmf2mod.cpp
	$(CC) $(FLAGS) dmf2mod.cpp

converter.o: converter.cpp
	$(CC) $(FLAGS) converter.cpp

modules.o: modules.cpp
	$(CC) $(FLAGS) modules.cpp

mod.o: mod.cpp
	$(CC) $(FLAGS) mod.cpp

dmf.o: dmf.cpp
	$(CC) $(FLAGS) dmf.cpp

.PHONY: clean zlibclean zlib

zlib zlib/libz.a:
	$(ZLIB_MAKE)

clean:
	$(RM) $(OUT) $(OBJS)

zlibclean:
	$(ZLIB_CLEAN)
