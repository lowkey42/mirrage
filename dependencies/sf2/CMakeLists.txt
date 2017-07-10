cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(sf2 VERSION 1.0 LANGUAGES CXX)

add_library(sf2 INTERFACE)
target_include_directories(sf2 INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

install(TARGETS sf2 EXPORT sf2_targets INCLUDES DESTINATION include)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h*")
install(EXPORT sf2_targets
	FILE sf2.cmake
	NAMESPACE sf2::
	DESTINATION lib/cmake/sf2
)

option(SF2_BUILD_TESTS "Build tests" off)

if(SF2_BUILD_TESTS)
	enable_testing()

	MESSAGE("Building tests for sf2")

	if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
		message(STATUS "Setting build type to 'Debug' as none was specified.")
		set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
	endif()


	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wextra -Wall -pedantic -Werror")

	add_executable(sf2_test_simple "tests/test_simple.cpp")
	target_link_library(sf2_test_simple sf2)
	add_executable(sf2_test_advanced "tests/test_advanced.cpp")
	target_link_library(sf2_test_advanced sf2)

	add_test(NAME simple_usage   COMMAND test_simple)
	add_test(NAME advanced_usage COMMAND test_advanced)
endif()
