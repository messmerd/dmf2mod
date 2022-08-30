cmake_minimum_required(VERSION 3.14)

project(dmf2mod VERSION "0.1.1")

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SRC ${PROJECT_SOURCE_DIR}/src)
configure_file(${PROJECT_SOURCE_DIR}/include/version.h.in ${PROJECT_SOURCE_DIR}/include/version.h)

# set sources
set(CONSOLE_SOURCES
    ${SRC}/console/console.cpp
    ${SRC}/utils/utils.cpp
)

set(WEB_APP_SOURCES
    ${SRC}/webapp/webapp.cpp
    ${SRC}/utils/utils.cpp
)

set(CORE_SOURCES
    ${SRC}/dmf2mod.cpp
    ${SRC}/core/module.cpp
    ${SRC}/core/module_base.cpp
    ${SRC}/core/conversion_options.cpp
    ${SRC}/core/conversion_options_base.cpp
    ${SRC}/core/status.cpp
    ${SRC}/core/registrar.cpp
    ${SRC}/core/options.cpp
    ${SRC}/core/global_options.cpp
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
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Asan" "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Set up address sanitizer
if(CMAKE_BUILD_TYPE STREQUAL "Asan")
    message(STATUS "Using address sanitizer")
    add_compile_options(${CMAKE_CXX_FLAGS_DEBUG})
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()

# Display default configuration-specific flags (debugging and optimization flags)
message(STATUS "Debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Release flags: ${CMAKE_CXX_FLAGS_RELEASE}")

# Compiler-specific warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WARNING_FLAGS "-Wall -Wno-unknown-pragmas -Werror -Wno-error=cpp -Wno-error=unused-variable")
elseif(EMSCRIPTEN)
    set(WARNING_FLAGS "-Wall -Wundefined-func-template -Wno-unknown-pragmas -Wno-\#warnings -Werror -Wno-error=unused-variable")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WARNING_FLAGS "-Wall -Wno-unknown-pragmas -Wno-\#warnings -Werror -Wno-error=unused-variable")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(WARNING_FLAGS "/W3") #/WX
else()
    set(WARNING_FLAGS "")
endif()

# WebAssembly flags
set(WASM_COMMON_FLAGS "-fexceptions -s DISABLE_EXCEPTION_CATCHING=0")
set(WASM_COMPILE_FLAGS "-s INLINING_LIMIT=1")
set(WASM_LINKER_FLAGS "-s USE_ZLIB=1 -s ASSERTIONS=1 -s MODULARIZE=0 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s NO_EXIT_RUNTIME=1")
set(WASM_LINKER_FLAGS "${WASM_LINKER_FLAGS} -s FORCE_FILESYSTEM=1 -s EXPORTED_RUNTIME_METHODS=\"['FS']\"")

set(WASM_FLAGS "${WASM_COMMON_FLAGS} ${WASM_COMPILE_FLAGS}")
set(WASM_LFLAGS "--bind -lidbfs.js ${WASM_LINKER_FLAGS} ${WASM_COMMON_FLAGS}")

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
    
    # Set compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WASM_FLAGS} ${WARNING_FLAGS}")
    message(STATUS "em++ compile flags: ${CMAKE_CXX_FLAGS}")

    # Only CMAKE_CXX_FLAGS and the LINK_FLAGS property have been working for me. Don't bother with EMCC_CFLAGS or CMAKE_EXE_LINKER_FLAGS.

    add_executable(dmf2mod ${WEB_APP_SOURCES} ${CORE_SOURCES} ${MODULE_SOURCES})

    # Set linker flags
    set_target_properties(dmf2mod PROPERTIES LINK_FLAGS "${WASM_LFLAGS} ${USE_WASM_FLAG} ${WARNING_FLAGS} --pre-js ${SRC}/webapp/pre.js")
    message(STATUS "em++ linker flags: ${WASM_LFLAGS} ${USE_WASM_FLAG} ${WARNING_FLAGS} --pre-js ${SRC}/webapp/pre.js")

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
    target_include_directories(dmf2mod PRIVATE ${ZLIB_DEPS} ${PROJECT_BINARY_DIR}/extern/zlib)
    target_link_libraries(${PROJECT_NAME} zlibstatic)
endif()

# includes
target_include_directories(dmf2mod PRIVATE
        ${PROJECT_SOURCE_DIR}/extern
        ${PROJECT_SOURCE_DIR}/extern/zstr
        ${PROJECT_SOURCE_DIR}/extern/zlib
        ${PROJECT_SOURCE_DIR}/extern/gcem/include
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/core
        ${PROJECT_SOURCE_DIR}/include/modules
        ${PROJECT_SOURCE_DIR}/include/utils
        ${PROJECT_BINARY_DIR}
)

target_include_directories(dmf2mod PUBLIC
    ${PROJECT_BINARY_DIR}
)
