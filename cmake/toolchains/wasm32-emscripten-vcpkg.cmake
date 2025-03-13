if(DEFINED ENV{VCPKG_ROOT})
	set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
elseif(DEFINED ENV{VCPKG_INSTALLATION_ROOT})
	set(VCPKG_ROOT "$ENV{VCPKG_INSTALLATION_ROOT}")
else()
	message(FATAL_ERROR "Please set the VCPKG_ROOT environment variable")
endif()

if(DEFINED ENV{EMSDK})
	set(EMSDK "$ENV{EMSDK}")
else()
	message(FATAL_ERROR "Please set the EMSDK environment variable to the Emscripten SDK path")
endif()

include(${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake)

set(VCPKG_TARGET_TRIPLET wasm32-emscripten)
include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
