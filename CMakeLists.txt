cmake_minimum_required(VERSION 3.14)

project(dmf2mod VERSION "0.1.1")

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(DMF2MOD_ROOT ${PROJECT_SOURCE_DIR})
set(SRC ${DMF2MOD_ROOT}/src)
set(INC ${DMF2MOD_ROOT}/include)

# Configure version
configure_file(${INC}/version.h.in ${INC}/version.h)

# Source files
set(CORE_SOURCES
    ${SRC}/dmf2mod.cpp
    ${SRC}/core/conversion_options.cpp
    ${SRC}/core/factory.cpp
    ${SRC}/core/global_options.cpp
    ${SRC}/core/module.cpp
    ${SRC}/core/options.cpp
    ${SRC}/core/status.cpp
)

set(MODULE_SOURCES
    ${SRC}/modules/dmf.cpp
    ${SRC}/modules/mod.cpp
)

set(UTILS_SOURCES
    ${SRC}/utils/utils.cpp
)

set(DMF2MOD_SOURCES ${CORE_SOURCES} ${MODULE_SOURCES} ${UTILS_SOURCES})

set(INC_DIRS
    ${DMF2MOD_ROOT}/extern
    ${DMF2MOD_ROOT}/extern/zstr
    ${DMF2MOD_ROOT}/extern/zlib
    ${DMF2MOD_ROOT}/extern/gcem/include
    ${DMF2MOD_ROOT}/include
    ${PROJECT_BINARY_DIR}
)

option(BUILD_CONSOLE "Build dmf2mod command-line application" TRUE)
option(ENABLE_CPPCHECK "Enable testing with cppcheck" FALSE)

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

# Set up Cppcheck static code analysis
if(ENABLE_CPPCHECK)
    find_program(CPPCHECK cppcheck)
    if(CPPCHECK)
    set(CMAKE_CXX_CPPCHECK
        ${CPPCHECK}
        --suppress=syntaxError
        --enable=all
        --inconclusive)
    else()
    message(SEND_ERROR "cppcheck requested but executable not found")
    endif()
endif()


if(EMSCRIPTEN)
    # Web app build
    add_subdirectory(webapp)
else()
    # DMF2MOD library build

    # Compiler-specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(WARNING_FLAGS "-Wall -Wno-unknown-pragmas -Werror -Wno-error=cpp -Wno-error=unused-variable")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(WARNING_FLAGS "-Wall -Wno-unknown-pragmas -Wno-\#warnings -Werror -Wno-error=unused-variable")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(WARNING_FLAGS "/W3") #/WX
    else()
        set(WARNING_FLAGS "")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS}")
    add_library(${PROJECT_NAME} STATIC ${DMF2MOD_SOURCES})

    #set_target_properties(${PROJECT_NAME} PROPERTIES
    #        ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

    # Build and link to the bundled zlib
    set(ZLIB_ROOT ${DMF2MOD_ROOT}/extern/zlib)
    add_subdirectory(${ZLIB_ROOT} EXCLUDE_FROM_ALL)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_DEPS} ${PROJECT_BINARY_DIR}/extern)
    target_link_libraries(${PROJECT_NAME} PRIVATE zlibstatic)

    # Includes
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${INC_DIRS}
    )

    target_include_directories(${PROJECT_NAME} PUBLIC
        ${INC}
        ${PROJECT_BINARY_DIR}
    )

    if(BUILD_CONSOLE)
        add_subdirectory(console)
    endif()
endif()
