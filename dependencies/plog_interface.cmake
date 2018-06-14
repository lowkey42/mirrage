cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(plog)

add_library(plog INTERFACE)
target_include_directories(plog SYSTEM INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/plog/include>
	$<INSTALL_INTERFACE:include>)
install(TARGETS plog EXPORT plogTargets)

export(
	EXPORT plogTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/plogTargets.cmake"
)

install(
	EXPORT plogTargets FILE plogTargets.cmake
	NAMESPACE plog::
	DESTINATION lib/cmake
)
