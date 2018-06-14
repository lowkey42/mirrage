cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(robin-map)

add_library(robin-map INTERFACE)
target_include_directories(robin-map SYSTEM INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/robin-map>
	$<INSTALL_INTERFACE:include>)
install(TARGETS robin-map EXPORT robin-mapTargets)

export(
	EXPORT robin-mapTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/robin-mapTargets.cmake"
)

install(
	EXPORT robin-mapTargets FILE robin-mapTargets.cmake
	NAMESPACE robin-map::
	DESTINATION lib/cmake
)
