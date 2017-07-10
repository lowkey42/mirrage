cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(gsl)

add_library(gsl INTERFACE)
target_include_directories(gsl INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/gsl/include>
	$<INSTALL_INTERFACE:include>)
install(TARGETS gsl EXPORT gslTargets)

export(
	EXPORT gslTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/gslTargets.cmake"
)

install(
	EXPORT gslTargets FILE gslTargets.cmake
	NAMESPACE gsl::
	DESTINATION lib/cmake
)
