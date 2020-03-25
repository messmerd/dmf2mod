OBJS	= dmf2mod.o mod.o dmf.o
SOURCE	= dmf2mod.c mod.c dmf.c
HEADER	= mod.h dmf.h zconf.h zlib.h

ifeq ($(OS),Windows_NT)
OUT	= dmf2mod.exe
else
OUT	= dmf2mod
endif

CC	 = gcc
FLAGS	 = -Izlib -g -c -Wall -Wno-unknown-pragmas
LFLAGS	 = -lm zlib/libz.a

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

dmf2mod.o: dmf2mod.c
	$(CC) $(FLAGS) dmf2mod.c 

mod.o: mod.c
	$(CC) $(FLAGS) mod.c 

dmf.o: dmf.c
	$(CC) $(FLAGS) dmf.c 


clean:
	$(RM) $(OUT) $(OBJS) 

