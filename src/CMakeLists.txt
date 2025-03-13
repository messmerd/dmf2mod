set(SRC "${CMAKE_CURRENT_SOURCE_DIR}")

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
	${SRC}/modules/debug.cpp
)

set(UTILS_SOURCES
	${SRC}/utils/utils.cpp
)

set(DMF2MOD_SOURCES
	${CORE_SOURCES}
	${MODULE_SOURCES}
	${UTILS_SOURCES}
	PARENT_SCOPE
)
