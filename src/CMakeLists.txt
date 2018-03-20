cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		option(MIRRAGE_SAN "Build with sanitizers" OFF)
		if(MIRRAGE_SAN)
			MESSAGE("Building with sanitizers")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=integer -fsanitize=undefined -fsanitize-address-use-after-scope ")
		endif()
		
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gline-tables-only") # for stack traces
	endif()
endif()


if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(MIRRAGE_DEFAULT_COMPILER_ARGS -Wextra -Wall -pedantic -Wextra-semi
		-Wzero-as-null-pointer-constant -Wold-style-cast -Werror
		-Wno-unused-parameter -Wno-unused-private-field -Wno-missing-braces)

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(MIRRAGE_DEFAULT_COMPILER_ARGS -Wextra -Wall -pedantic -Wunsafe-loop-optimizations
		-Wzero-as-null-pointer-constant -Wlogical-op -Werror -Wno-unused-parameter
		-Wno-missing-braces
		-Wno-error=unsafe-loop-optimizations)

elseif(MSVC)
	set(MIRRAGE_DEFAULT_COMPILER_ARGS /Za)
endif()

if(MIRRAGE_BUILD_DEMO)
	add_subdirectory(demo)
endif()

if(MIRRAGE_BUILD_MESH_CONVERTER)
	add_subdirectory(mesh_converter)
endif()

add_subdirectory(mirrage)

option(MIRRAGE_ENABLE_CLANG_FORMAT "Includes a clangformat target, that automatically formats the source files." OFF)
if(MIRRAGE_ENABLE_CLANG_FORMAT)
	include(clang-format.cmake)
endif()
