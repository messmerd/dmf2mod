OBJS	= dmf2mod.o mod.o dmf.o
SOURCE	= dmf2mod.cpp mod.cpp dmf.cpp
HEADER	= mod.h dmf.h zconf.h zlib.h

ifeq ($(OS),Windows_NT)
OUT	= dmf2mod.exe
else
OUT	= dmf2mod
endif

CC	 = g++
FLAGS	 = -Izlib -g -c -Wall -Wno-unknown-pragmas
LFLAGS	 = -lm zlib/libz.a

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

dmf2mod.o: dmf2mod.cpp
	$(CC) $(FLAGS) dmf2mod.cpp

mod.o: mod.cpp
	$(CC) $(FLAGS) mod.cpp

dmf.o: dmf.cpp
	$(CC) $(FLAGS) dmf.cpp


clean:
	$(RM) $(OUT) $(OBJS)

