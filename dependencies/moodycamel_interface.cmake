cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(moodycamel LANGUAGES CXX)

add_library(moodycamel INTERFACE)
add_library(moodycamel::moodycamel ALIAS moodycamel)
target_include_directories(moodycamel SYSTEM INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moodycamel>
	$<INSTALL_INTERFACE:include>)


install(TARGETS moodycamel EXPORT moodycamelTargets INCLUDES DESTINATION include)
export(
	EXPORT moodycamelTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/moodycamelTargets.cmake"
)

install(
	EXPORT moodycamelTargets FILE moodycamelTargets.cmake
	DESTINATION lib/cmake
)

