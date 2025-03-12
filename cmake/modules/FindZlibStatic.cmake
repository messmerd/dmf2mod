# FindZlibStatic.cmake

set(ZLIB_USE_STATIC_LIBS ON)
find_package(ZLIB 1.2.3 QUIET)

if(NOT ZLIB_FOUND)
	message(STATUS "Downloading zlib using FetchContent")

	include(FetchContent)
	FetchContent_Declare(
		zlibGitRepo
		GIT_REPOSITORY "https://github.com/madler/zlib"
		GIT_TAG        "develop"
	)

	set(ZLIB_BUILD_SHARED OFF)
	set(ZLIB_BUILD_TESTING OFF)
	FetchContent_MakeAvailable(zlibGitRepo)

	if(TARGET zlibstatic)
		set(ZLIB_FOUND TRUE)
		set(ZLIB_LIBRARY zlibstatic)
		get_target_property(ZLIB_INCLUDE_DIRS zlibstatic INTERFACE_INCLUDE_DIRECTORIES)

		# The shared library was not built, and the zlibstatic target
		# unfortunately does not provide the version - hence this workaround
		get_target_property(_zlib_source_dir zlibstatic SOURCE_DIR)
		file(STRINGS "${_zlib_source_dir}/zlib.h" _zlib_version_line REGEX "#define ZLIB_VERSION")
		string(REGEX REPLACE ".*#define ZLIB_VERSION \"([0-9\.]+).*" "\\1" ZLIB_VERSION "${_zlib_version_line}")

		# Normally ZLIB::ZLIB is the shared library,
		# but this will force zstr to use the static library
		add_library(ZLIB::ZLIB ALIAS zlibstatic)
	else()
		message(WARNING "Failed to download zlib")
		return()
	endif()
endif()

if(TARGET zlibstatic)
	# Ensure any future calls to find_package(ZLIB) find the zlib
	# found by this module
	get_target_property(ZLIB_ROOT zlibstatic SOURCE_DIR)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZLIB
	REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIRS
	VERSION_VAR ZLIB_VERSION
	NAME_MISMATCHED
)
