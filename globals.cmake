cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# required at top-level
set(MIRRAGE_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})
set(MIRRAGE_ROOT_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${MIRRAGE_ROOT_DIR}/modules")

enable_language(C CXX ASM)

add_definitions(-DGSL_TERMINATE_ON_CONTRACT_VIOLATION)

if (WIN32)
	option(MIRRAGE_ENABLE_BACKWARD "Enable stacktraces through backward-cpp" OFF)
else()
	option(MIRRAGE_ENABLE_BACKWARD "Enable stacktraces through backward-cpp" ON)
endif()

# LTO
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024 -fno-strict-aliasing")

	option(MIRRAGE_ENABLE_LTO "Enable link-time optimization" OFF)
	if(MIRRAGE_ENABLE_LTO)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
	endif()
endif()

# Sanitizers
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

# Default compiler flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(MIRRAGE_DEFAULT_COMPILER_ARGS -Wextra -Wall -pedantic -Wextra-semi
		-Wzero-as-null-pointer-constant -Wold-style-cast -Werror
		-Wno-unused-parameter -Wno-unused-private-field -Wno-missing-braces -Wno-error-unused-command-line-argument)

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(MIRRAGE_DEFAULT_COMPILER_ARGS -Wextra -Wall -pedantic
		-Wlogical-op -Werror -Wno-unused-parameter
		-Wno-missing-braces)

elseif(MSVC)
	set(MIRRAGE_DEFAULT_COMPILER_ARGS /DWIN32_LEAN_AND_MEAN /DNOMINMAX /MP /W3 /WX)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4221")
	set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")
endif()

# Select optimal linker
if (UNIX AND NOT APPLE)
	execute_process(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=lld -Wl,--version ERROR_QUIET OUTPUT_VARIABLE ld_version)
	if ("${ld_version}" MATCHES "LLD")
		message("using LLD linker")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -Wl,--threads,--build-id=none")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld -Wl,--threads,--build-id=none")

	else()
		message("OUT ${ld_version}")
		execute_process(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version ERROR_QUIET OUTPUT_VARIABLE ld_version)
		if ("${ld_version}" MATCHES "GNU gold")
			message("using gold linker")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold -Wl,--disable-new-dtags")
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold -Wl,--disable-new-dtags")
		else()
			message("using default linker")
		endif()
	endif()
endif()

option(MIRRAGE_FORCE_LIBCPP "Force usage of libc++ instead of libstdc++ intependent of the used compiler. " OFF)
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR MIRRAGE_FORCE_LIBCPP)
	option(MIRRAGE_USE_LIBCPP "Uses libc++ instead of libstdc++. " ON)
	if(MIRRAGE_USE_LIBCPP OR MIRRAGE_FORCE_LIBCPP)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
	endif()
endif()

option(MIRRAGE_OPTIMIZE_NATIVE "Enable -march=native" OFF)
if(${MIRRAGE_OPTIMIZE_NATIVE})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

option(MIRRAGE_ENABLE_COTIRE "Enable cotire" ON)
if(MIRRAGE_ENABLE_COTIRE)
	include(cotire OPTIONAL)

	if(COMMAND cotire)
		add_definitions(-DGLM_FORCE_RADIANS -DGLM_FORCE_DEPTH_ZERO_TO_ON -DGLM_ENABLE_EXPERIMENTAL -DGLM_FORCE_CXX14)
		if(NOT MSVC)
			add_compile_options(-pthread)
		endif()
		set_property(GLOBAL PROPERTY COTIRE_PREFIX_HEADER_INCLUDE_PATH "${MIRRAGE_ROOT_DIR}/dependencies")
		set_property(GLOBAL PROPERTY COTIRE_PREFIX_HEADER_IGNORE_PATH "${MIRRAGE_ROOT_DIR}/dependencies/imgui;${MIRRAGE_ROOT_DIR}/src;${MIRRAGE_ROOT_DIR}/dependencies/moodycamel/concurrentqueue.h")
		set_property(GLOBAL PROPERTY COTIRE_ADD_UNITY_BUILD FALSE)
	endif()
endif()


option(MIRRAGE_ENABLE_TESTS "Enable unit tests" ON)
if(MIRRAGE_ENABLE_TESTS)
	enable_testing()
endif()

option(MIRRAGE_ENABLE_CLANG_FORMAT "Includes a clangformat target, that automatically formats the source files." OFF)
if(MIRRAGE_ENABLE_CLANG_FORMAT)
	include(${MIRRAGE_ROOT_DIR}/clang-format.cmake)
endif()

include(${MIRRAGE_ROOT_DIR}/embed_assets.cmake)

