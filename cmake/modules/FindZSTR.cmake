# FindZSTR.cmake

find_package(ZlibStatic)

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
		set(ZSTR_FOUND TRUE)
		get_target_property(ZSTR_INCLUDE_DIRS zstr::zstr INTERFACE_INCLUDE_DIRECTORIES)
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZSTR REQUIRED_VARS ZSTR_INCLUDE_DIRS)
