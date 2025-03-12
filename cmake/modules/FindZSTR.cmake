# FindZSTR.cmake

set(ZLIB_USE_STATIC_LIBS ON)
find_package(ZLIB 1.2.3)

if(NOT ZLIB_FOUND)
	message(STATUS "Downloading zlib using FetchContent")

	include(FetchContent)
	FetchContent_Declare(
		zlibGitRepo
		GIT_REPOSITORY "https://github.com/madler/zlib"
		GIT_TAG        "master"
	)
	FetchContent_MakeAvailable(zlibGitRepo)

	if(TARGET zlibstatic)
		set(ZLIB_FOUND TRUE)
	else()
		message(WARNING "Failed to download zlib")
		return()
	endif()
endif()

if(DEFINED VCPKG_TOOLCHAIN)
	# Use vcpkg when available
	find_path(ZSTR_INCLUDE_DIRS "zstr.hpp")

	# Manually create the CMake target
	if(ZSTR_INCLUDE_DIRS)
		add_library(zstr::zstr INTERFACE IMPORTED)
		target_include_directories(zstr::zstr INTERFACE "${ZSTR_INCLUDE_DIRS}")
		target_link_libraries(zstr::zstr INTERFACE ZLIB::ZLIB)
		target_compile_features(zstr::zstr INTERFACE cxx_std_17)
	endif()
else()
	message(STATUS "Downloading zstr using FetchContent")

	include(FetchContent)
	FetchContent_Declare(ZStrGitRepo
		GIT_REPOSITORY "https://github.com/mateidavid/zstr"
		GIT_TAG        "master"
	)
	FetchContent_MakeAvailable(ZStrGitRepo)

	if(TARGET zstr::zstr)
		set(Foo_FOUND TRUE)
		return()
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZSTR REQUIRED_VARS ZSTR_INCLUDE_DIRS)

mark_as_advanced(ZSTR_INCLUDE_DIRS)
