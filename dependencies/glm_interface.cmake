cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(glm)

add_library(glm INTERFACE)
target_include_directories(glm SYSTEM INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glm/include>
	$<INSTALL_INTERFACE:include>)
install(TARGETS glm EXPORT glmTargets)

export(
	EXPORT glmTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/glmTargets.cmake"
)

install(
	EXPORT glmTargets FILE glmTargets.cmake
	NAMESPACE glm::
	DESTINATION lib/cmake
)
