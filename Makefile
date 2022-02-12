CORE_OBJS	= core.o modules.o mod.o dmf.o
WASM_CORE_OBJS	= webapp/core.o webapp/modules.o webapp/mod.o webapp/dmf.o

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

# Compiler and linker flags
FLAGS	= -std=c++17 -Izlib -Izstr -Wall -Wno-unknown-pragmas
LFLAGS	= -lm zlib/libz.a

# Emscripten compiler and linker flags
WASM_LDFLAGS	= -O3
WASM_DEFINES	= -s USE_ZLIB=1 -s INLINING_LIMIT=1
WASM_LDEFINES	= $(WASM_DEFINES) -s ASSERTIONS=1 -s MODULARIZE=0 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s NO_EXIT_RUNTIME=1 -s FORCE_FILESYSTEM=1 -s EXPORTED_FUNCTIONS="['_main']" -s EXPORTED_RUNTIME_METHODS='["FS"]' -s NO_DISABLE_EXCEPTION_CATCHING

WASM_FLAGS	= -c -std=c++17 -Izstr -Wall -Wno-unknown-pragmas $(WASM_LDFLAGS) $(WASM_DEFINES)
WASM_LFLAGS	= -std=c++17 --bind -lidbfs.js -Izstr -Wall -Wno-unknown-pragmas $(WASM_LDFLAGS) $(WASM_LDEFINES)

# Build command-line program by default
all: cmd_program

# Command-line program:
cmd_program: core dmf2mod.o
	$(CC) -g $(CORE_OBJS) dmf2mod.o -o $(OUT) $(LFLAGS)
	@echo "Done building dmf2mod"

dmf2mod.o: dmf2mod.cpp
	$(CC) -c -g $(FLAGS) dmf2mod.cpp

# Web App, WASM version:
webapp: core_wasm webapp.cpp
	$(WASM_CC) webapp.cpp -s WASM=1 $(WASM_LFLAGS) $(WASM_CORE_OBJS) --pre-js webapp/pre.js -o webapp/dmf2mod.js
	@echo "Done building web app"

# Web App, asm.js version:
#	$(WASM_CC) webapp.cpp -s WASM=0 $(WASM_LFLAGS) $(WASM_CORE_OBJS) --pre-js webapp/pre.js -o webapp/dmf2mod.asm.js

# Dmf2mod core:
core: zlib $(CORE_OBJS)
	@echo "Done building core"

%.o : %.cpp
	$(CC) -c -g $(FLAGS) $< -o $@

# Dmf2mod WebAssembly core:
core_wasm: $(WASM_CORE_OBJS)
	@echo "Done building WebAssembly core"

webapp/%.o: %.cpp
	$(WASM_CC) -c $(WASM_FLAGS) $< -o $@

# 3rd Party:
zlib/libz.a:
	$(ZLIB_MAKE)
	@echo "Done building zlib"

zlib: zlib/libz.a

.PHONY: cmd_program webapp zlib clean clean_zlib

clean:
	$(RM) $(OUT) $(CORE_OBJS) $(WASM_CORE_OBJS) dmf2mod.o webapp/dmf2mod.wasm webapp/dmf2mod.js webapp/dmf2mod.asm.js

clean_zlib:
	$(ZLIB_CLEAN)
