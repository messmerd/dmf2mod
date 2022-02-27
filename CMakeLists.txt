cmake_minimum_required(VERSION 3.11)

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

# Set default build type if none was specified
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Default debug and optimize flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

if(NOT MSVC)
    set(WARNING_FLAGS "-Wall -Wno-unknown-pragmas")
else()
    set(WARNING_FLAGS "")
endif()

# WASM flags
set(WASM_LDFLAGS "")
set(WASM_DEFINES "-s USE_ZLIB=1 -s INLINING_LIMIT=1")
set(WASM_LDEFINES "${WASM_DEFINES} -s ASSERTIONS=1 -s MODULARIZE=0 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s NO_EXIT_RUNTIME=1 -s FORCE_FILESYSTEM=1")
set(WASM_LDEFINES "${WASM_LDEFINES} -s EXPORTED_FUNCTIONS=\"['_main']\" -s EXPORTED_RUNTIME_METHODS='[\"FS\"]' -s NO_DISABLE_EXCEPTION_CATCHING")

set(WASM_FLAGS "${WASM_LDFLAGS} ${WASM_DEFINES}")
set(WASM_LFLAGS "--bind -lidbfs.js ${WASM_LDFLAGS} ${WASM_LDEFINES}")

# EMSCRIPTEN is defined when using emcmake
if(EMSCRIPTEN)
    # Web app
    message(STATUS "Setting up for web app")
    
    # Determine whether to build WASM or asm.js version
    option(USE_WASM "Use WASM (asm.js is used otherwise)" ON)
    if (USE_WASM)
        message(STATUS "Using WebAssembly")
        set(USE_WASM_FLAG "-s WASM=1")
    else()
        message(STATUS "Using asm.js")
        set(USE_WASM_FLAG "-s WASM=0")
    endif()
    
    # Set compiler and linker flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WASM_FLAGS} ${WARNING_FLAGS} --pre-js ${SRC}/webapp/pre.js" CACHE STRING "")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${USE_WASM_FLAG} ${WASM_LFLAGS}" CACHE STRING "")

    add_executable(dmf2mod ${WEB_APP_SOURCES} ${CORE_SOURCES} ${MODULE_SOURCES})

    include_directories(${ZLIB_INCLUDE_DIRS})

    add_custom_command(
    TARGET dmf2mod 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SRC}/webapp/ui"
        "${CMAKE_BINARY_DIR}"
    )
else()
    # Console application
    message(STATUS "Setting up for console application")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS}")

    add_executable(dmf2mod ${CONSOLE_SOURCES} ${CORE_SOURCES} ${MODULE_SOURCES})

    # zlib
    set(ZLIB_ROOT ${PROJECT_SOURCE_DIR}/extern/zlib)
    add_subdirectory(${ZLIB_ROOT} EXCLUDE_FROM_ALL)
    include_directories(${ZLIB_DEPS})
    target_link_libraries(${PROJECT_NAME} zlibstatic)
endif()

# includes
include_directories(PRIVATE
        ${PROJECT_SOURCE_DIR}/extern
        ${PROJECT_SOURCE_DIR}/extern/zstr
        ${PROJECT_SOURCE_DIR}/extern/zlib
        ${PROJECT_BINARY_DIR}
        ${PROJECT_BINARY_DIR}/extern/zlib
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/modules
)

include_directories(PUBLIC
    ${PROJECT_BINARY_DIR}
)
