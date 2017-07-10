cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		option(SAN "Build with sanitizers" OFF)
		if(SAN)
			MESSAGE("Building with sanitizers")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=integer -fsanitize=undefined -fsanitize-address-use-after-scope ")
		endif()
		
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gline-tables-only") # for stack traces
	endif()
endif()


add_subdirectory(demo)
add_subdirectory(mesh_converter)
add_subdirectory(mirrage)

