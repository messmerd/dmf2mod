message(STATUS "Setting up for web app")

project(dmf2mod_webapp)

# Source files
set(WEBAPP_SOURCES
	${DMF2MOD_ROOT}/webapp/webapp.cpp
)

# Warnings
set(WARNING_FLAGS "-Wall -Wundefined-func-template -Wno-unknown-pragmas -Wno-\#warnings -Werror -Wno-error=unused-variable -Wno-deprecated-non-prototype")

# WebAssembly flags
set(WASM_COMMON_FLAGS "-fexceptions -s DISABLE_EXCEPTION_CATCHING=0")
set(WASM_COMPILE_FLAGS "-s INLINING_LIMIT=1")
set(WASM_LINKER_FLAGS "-s USE_ZLIB=0 -s ASSERTIONS=1 -s MODULARIZE=0 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s NO_EXIT_RUNTIME=1")
set(WASM_LINKER_FLAGS "${WASM_LINKER_FLAGS} -s FORCE_FILESYSTEM=1 -s EXPORTED_RUNTIME_METHODS=\"['FS']\"")

set(WASM_FLAGS "${WASM_COMMON_FLAGS} ${WASM_COMPILE_FLAGS}")
set(WASM_LFLAGS "--bind -lidbfs.js ${WASM_LINKER_FLAGS} ${WASM_COMMON_FLAGS}")

# Determine whether to build WASM or asm.js version
option(USE_WASM "Use WASM (asm.js is used otherwise)" ON)
if (USE_WASM)
	message(STATUS "Using WebAssembly")
	set(USE_WASM_FLAG "-s WASM=1")
else()
	message(STATUS "Using asm.js")
	set(USE_WASM_FLAG "-s WASM=0")
endif()

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WASM_FLAGS} ${WARNING_FLAGS}")
message(STATUS "em++ compile flags: ${CMAKE_CXX_FLAGS}")

# Only CMAKE_CXX_FLAGS and the LINK_FLAGS property have been working for me. Don't bother with EMCC_CFLAGS or CMAKE_EXE_LINKER_FLAGS.

add_executable(${PROJECT_NAME} ${DMF2MOD_SOURCES} ${WEBAPP_SOURCES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME dmf2mod)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/html)

# Set linker flags
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "${WASM_LFLAGS} ${USE_WASM_FLAG} ${WARNING_FLAGS} --pre-js ${DMF2MOD_ROOT}/webapp/pre.js")
message(STATUS "em++ linker flags: ${WASM_LFLAGS} ${USE_WASM_FLAG} ${WARNING_FLAGS} --pre-js ${DMF2MOD_ROOT}/webapp/pre.js")

target_link_libraries(${PROJECT_NAME} PRIVATE gcem zstr::zstr)
target_include_directories(${PROJECT_NAME} PUBLIC ${DMF2MOD_ROOT}/include)

add_custom_command(
	TARGET ${PROJECT_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${DMF2MOD_ROOT}/webapp/ui"
		"${CMAKE_BINARY_DIR}/html"
)
