cmake_minimum_required(VERSION 3.24)

project(dmf2mod VERSION "0.2.0")
set(PROJECT_VERSION_STAGE "alpha")
if(PROJECT_VERSION_STAGE)
	set(PROJECT_VERSION "${PROJECT_VERSION}-${PROJECT_VERSION_STAGE}")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

######################
## Set build target ##
######################

set(BUILD_TARGET "console" CACHE STRING "Choose build type: library, console, or web-app")
set_property(CACHE BUILD_TARGET PROPERTY STRINGS "library" "console" "web-app")

if(BUILD_TARGET STREQUAL "library")
	if(DEFINED EMSCRIPTEN)
		message(FATAL_ERROR "Cannot build library using Emscripten")
	endif()
	message(STATUS "Building dmf2mod library")
elseif(BUILD_TARGET STREQUAL "console")
	set(BUILD_CONSOLE 1)
	if(DEFINED EMSCRIPTEN)
		message(FATAL_ERROR "Cannot build command-line application using Emscripten")
	endif()
	message(STATUS "Building dmf2mod command-line application")
elseif(BUILD_TARGET STREQUAL "web-app")
	set(BUILD_WEBAPP 1)
	if(NOT DEFINED EMSCRIPTEN)
		message(FATAL_ERROR "Please use the wasm32-emscripten-vcpkg.cmake toolchain file")
	endif()
	message(STATUS "Building dmf2mod web app")
else()
	message(FATAL_ERROR "Invalid BUILD_TARGET: ${BUILD_TARGET}")
endif()

#############################
## Set build configuration ##
#############################

# Set default build type if none was specified
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified")
	set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build" FORCE)
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

###########################
## Static analysis setup ##
###########################

option(ENABLE_CPPCHECK "Enable testing with cppcheck" FALSE)

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

##################
## Dependencies ##
##################

find_package(gcem REQUIRED)
find_package(ZSTR REQUIRED)

###########
## Build ##
###########

set(DMF2MOD_ROOT ${PROJECT_SOURCE_DIR})

add_subdirectory(src)

# Configure version
configure_file(include/version.h.in include/version.h)

# Build web app
if(BUILD_WEBAPP)
	add_subdirectory(webapp)
	return()
endif()

# DMF2MOD library build

# Compiler-specific warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(WARNING_FLAGS -Wall -Wno-unknown-pragmas -Werror -Wno-error=cpp -Wno-error=unused-variable)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(WARNING_FLAGS -Wall -Wno-unknown-pragmas "-Wno-\#warnings" -Werror -Wno-error=unused-variable)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(WARNING_FLAGS /W3) #/WX
	set(OTHER_FLAGS /utf-8 /permissive-)
endif()

add_library(${PROJECT_NAME} STATIC ${DMF2MOD_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE gcem zstr::zstr)
target_include_directories(${PROJECT_NAME} PUBLIC ${DMF2MOD_ROOT}/include)
target_compile_options(${PROJECT_NAME} PRIVATE ${WARNING_FLAGS} ${OTHER_FLAGS})

#set_target_properties(${PROJECT_NAME} PROPERTIES
#	ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

if(BUILD_CONSOLE)
	add_subdirectory(console)
endif()
