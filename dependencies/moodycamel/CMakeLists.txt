cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(moodycamel LANGUAGES CXX)

add_library(moodycamel INTERFACE)
target_include_directories(moodycamel SYSTEM INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

install(TARGETS moodycamel EXPORT moodycamel_targets INCLUDES DESTINATION include)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h*")
install(EXPORT moodycamel_targets
	FILE moodycamel.cmake
	NAMESPACE moodycamel::
	DESTINATION lib/cmake
)

