cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(doctest)

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/doctest/doctest>
	$<INSTALL_INTERFACE:include>)
install(TARGETS doctest EXPORT doctestTargets)

export(
	EXPORT doctestTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/doctestTargets.cmake"
)

install(
	EXPORT doctestTargets FILE doctestTargets.cmake
	NAMESPACE doctest::
	DESTINATION lib/cmake
)
