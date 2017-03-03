cmake_minimum_required(VERSION 2.6)

project(sf2)

include_directories(AFTER include)

install(DIRECTORY include/sf2/ DESTINATION include/sf2 FILES_MATCHING PATTERN "*.h*")

option(SF2_BUILD_TESTS "Build tests" off)

if(SF2_BUILD_TESTS)
	enable_testing()

	MESSAGE("Building tests for sf2")

	if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
		message(STATUS "Setting build type to 'Debug' as none was specified.")
		set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
	endif()


	FILE(GLOB_RECURSE INC  "include/*.h*")

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wextra -Wall -pedantic -Werror")

	add_executable(test_simple
		"tests/test_simple.cpp"
		${INC}
	)
	add_executable(test_advanced
		"tests/test_advanced.cpp"
		${INC}
	)

	add_test(NAME simple_usage   COMMAND test_simple)
	add_test(NAME advanced_usage COMMAND test_advanced)
endif()
