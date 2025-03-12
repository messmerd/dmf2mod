# Findgcem.cmake

find_package(gcem CONFIG QUIET)

if(NOT gcem_FOUND)
	message(STATUS "Downloading gcem using FetchContent")

	include(FetchContent)
	FetchContent_Declare(
		gcemGitRepo
		GIT_REPOSITORY "https://github.com/kthohr/gcem"
		GIT_TAG        "master"
	)
	FetchContent_MakeAvailable(gcemGitRepo)

	if(TARGET gcem)
		set(gcem_FOUND TRUE)
	endif()
endif()

get_target_property(gcem_INCLUDE_DIR gcem INTERFACE_INCLUDE_DIRECTORIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gcem
	REQUIRED_VARS gcem_INCLUDE_DIR
	VERSION_VAR gcem_VERSION
)
