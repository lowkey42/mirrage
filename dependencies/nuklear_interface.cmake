cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(nuklear)

add_library(nuklear INTERFACE)
target_include_directories(nuklear SYSTEM INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/nuklear>
	$<INSTALL_INTERFACE:include>
)
install(TARGETS nuklear EXPORT nuklearTargets)

export(
	EXPORT nuklearTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/nuklearTargets.cmake"
)

install(
	EXPORT nuklearTargets FILE nuklearTargets.cmake
	NAMESPACE nuklear::
	DESTINATION lib/cmake
)
