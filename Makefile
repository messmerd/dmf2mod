CORE_OBJS	= core.o modules.o mod.o dmf.o
WASM_CORE_OBJS	= core.wasmo modules.wasmo mod.wasmo dmf.wasmo

ifeq ($(OS),Windows_NT)
OUT	= dmf2mod.exe
ZLIB_MAKE	= $(MAKE) --directory=zlib --makefile=win32/Makefile.gcc
ZLIB_CLEAN	= $(MAKE) clean --directory=zlib --makefile=win32/Makefile.gcc
WASM_CC	= em++.bat
else
OUT	= dmf2mod
ZLIB_MAKE	= cd zlib && chmod +x ./configure && ./configure --static && $(MAKE) && cd ..
ZLIB_CLEAN	= cd zlib && $(MAKE) clean && cd ..
WASM_CC	= em++
endif

CC	 = g++

# Compiler flags
FLAGS	= -c -std=c++17 -Izlib -Izstr -g -Wall -Wno-unknown-pragmas
WASM_FLAGS	= -c -std=c++17 -s USE_ZLIB=1 -Izlib -Izstr -g -Wall -Wno-unknown-pragmas

# Linker flags
LFLAGS	= -lm zlib/libz.a
WASM_LFLAGS	= -Izlib -Izstr --bind -s WASM=1 -s USE_ZLIB=1

# Build command-line program by default
all: cmd_program

# Command-line program:
cmd_program: core dmf2mod.o
	$(CC) -g $(CORE_OBJS) dmf2mod.o -o $(OUT) $(LFLAGS)
	@echo "Done building dmf2mod"

dmf2mod.o: dmf2mod.cpp
	$(CC) $(FLAGS) dmf2mod.cpp

# Web App:
webapp: core_wasm webapp.cpp
	$(WASM_CC) $(WASM_LFLAGS) webapp.cpp -g $(WASM_CORE_OBJS) -o webapp/dmf2mod.js
	@echo "Done building web app"

# Dmf2mod core:
core: zlib $(CORE_OBJS)
	@echo "Done building core"

%.o : %.cpp
	$(CC) -c $(FLAGS) $< -o $@

# Dmf2mod WebAssembly core:
core_wasm: $(WASM_CORE_OBJS)
	@echo "Done building WebAssembly core"

%.wasmo: %.cpp
	$(WASM_CC) -c $(WASM_FLAGS) $< -o $@

# 3rd Party:
zlib/libz.a:
	$(ZLIB_MAKE)
	@echo "Done building zlib"

zlib: zlib/libz.a

.PHONY: cmd_program webapp zlib clean clean_zlib

clean:
	$(RM) $(OUT) $(CORE_OBJS) $(WASM_CORE_OBJS) dmf2mod.o webapp/dmf2mod.wasm webapp/dmf2mod.js

clean_zlib:
	$(ZLIB_CLEAN)
