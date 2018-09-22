cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(glm)

add_library(glm INTERFACE)
add_library(glm::glm ALIAS glm)

target_include_directories(glm BEFORE INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glm>
	$<INSTALL_INTERFACE:include>)
install(TARGETS glm EXPORT glmTargets)

export(
	EXPORT glmTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/glmTargets.cmake"
)

install(
	EXPORT glmTargets FILE glmTargets.cmake
	DESTINATION lib/cmake
)
