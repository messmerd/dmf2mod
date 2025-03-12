# Findgcem.cmake

find_package(gcem CONFIG QUIET)

if(NOT gcem_FOUND)
	include(FetchContent)
	FetchContent_Declare(
		gcemGitRepo
		GIT_REPOSITORY "https://github.com/kthohr/gcem"
		GIT_TAG        "master"
	)
	FetchContent_MakeAvailable(gcemGitRepo)

	if(TARGET gcem)
		set(gcem_FOUND TRUE)
		return()
	endif()
endif()
