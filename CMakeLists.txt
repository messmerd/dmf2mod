cmake_minimum_required(VERSION 3.10)

project(dmf2mod VERSION "0.1")

enable_language(CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SRC ${PROJECT_SOURCE_DIR}/src)
configure_file(${PROJECT_SOURCE_DIR}/dmf2mod_config.h.in ${PROJECT_SOURCE_DIR}/include/dmf2mod_config.h)

# set sources
set(CONSOLE_SOURCES
    ${SRC}/dmf2mod.cpp
)

set(WEB_APP_SOURCES
    ${SRC}/webapp/webapp.cpp
)

set(CORE_SOURCES
    ${SRC}/modules.cpp
    ${SRC}/core.cpp
)

set(MODULE_SOURCES
    ${SRC}/modules/dmf.cpp
    ${SRC}/modules/mod.cpp
)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(WARNING_FLAGS -Wall -Wno-unknown-pragmas)

set(WASM_DEFINES "-s USE_ZLIB=1 -s INLINING_LIMIT=1")
set(WASM_LDEFINES "$(WASM_DEFINES) -s ASSERTIONS=1 -s MODULARIZE=0 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s NO_EXIT_RUNTIME=1 -s FORCE_FILESYSTEM=1")
set(WASM_LDEFINES "$(WASM_LDEFINES) -s EXPORTED_FUNCTIONS=\"['_main']\" -s EXPORTED_RUNTIME_METHODS='[\"FS\"]' -s NO_DISABLE_EXCEPTION_CATCHING)")

set(WASM_FLAGS "$(WASM_LDFLAGS) $(WASM_DEFINES)")
set(WASM_LFLAGS "--bind -lidbfs.js $(WASM_LDFLAGS) $(WASM_LDEFINES)")

add_compile_options(${WARNING_FLAGS})

# add the executable
if(NOT EMSCRIPTEN)
    # console application
    add_executable(dmf2mod ${CONSOLE_SOURCES} ${CORE_SOURCES} ${MODULE_SOURCES})

    # zlib
    set(ZLIB_ROOT ${PROJECT_SOURCE_DIR}/extern/zlib)
    add_subdirectory(${ZLIB_ROOT} EXCLUDE_FROM_ALL)
    include_directories(${ZLIB_DEPS})
    target_link_libraries(${PROJECT_NAME} zlibstatic)
else()
    # web app
    option(USE_WASM "Use WASM (asm.js is used otherwise)" ON)
    if (USE_WASM)
        message("Using WebAssembly")
        add_compile_options(-s WASM=1)
    else()
        message("Using asm.js")
        add_compile_options(-s WASM=0)
    endif()
    add_compile_options(${WASM_FLAGS})
    add_compile_options(--pre-js ${SRC}/webapp/pre.js)
    add_link_options(${WASM_LFLAGS})
    
    add_executable(webapp ${WEB_APP_SOURCES} ${CORE_SOURCES} ${MODULE_SOURCES})

    add_custom_command(
    TARGET webapp 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SRC}/webapp/ui"
        $<TARGET_FILE_DIR:webapp>
    )
endif()

# includes
include_directories(PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/modules
        ${PROJECT_SOURCE_DIR}/extern
        ${PROJECT_SOURCE_DIR}/extern/zstr
        ${ZLIB_ROOT}
        ${PROJECT_BINARY_DIR}/extern/zlib
)

include_directories(PUBLIC
    ${PROJECT_BINARY_DIR}
)
