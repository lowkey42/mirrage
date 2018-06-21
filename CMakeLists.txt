cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# required at top-level
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${ROOT_DIR}/modules")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024 -fno-strict-aliasing")

	option(MIRRAGE_ENABLE_LTO "Enable link-time optimization" OFF)
	if(MIRRAGE_ENABLE_LTO)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
	endif()
endif()

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


option(MIRRAGE_BUILD_MESH_CONVERTER "Build the mesh converter (requires assimp)" OFF)

get_directory_property(hasParent PARENT_DIRECTORY)
if(NOT hasParent)
	option(MIRRAGE_BUILD_DEMO "Build the demo application" ON)

	# custom target that does nothing but references all files so they show up in QtCreator
	file(GLOB_RECURSE ALL_FILES
			 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
			 *.*)

	list(FILTER ALL_FILES EXCLUDE REGEX "^src/.+\\.(h|c)+")
	list(FILTER ALL_FILES EXCLUDE REGEX "^src/.*CMakeLists.txt")
	add_custom_target(zz_all_project_files SOURCES ${ALL_FILES})

else()
	option(MIRRAGE_BUILD_DEMO "Build the demo application" OFF)
endif()


option(MIRRAGE_ENABLE_COTIRE "Enable cotire" ON)
if(MIRRAGE_ENABLE_COTIRE)
	include(cotire OPTIONAL)

	if(COMMAND cotire)
		add_definitions(-DGLM_FORCE_RADIANS -DGLM_FORCE_DEPTH_ZERO_TO_ON -DGLM_ENABLE_EXPERIMENTAL -DGLM_FORCE_CXX14)
		add_compile_options(-pthread)
		set_property(GLOBAL PROPERTY COTIRE_PREFIX_HEADER_INCLUDE_PATH "${ROOT_DIR}/dependencies")
		set_property(GLOBAL PROPERTY COTIRE_PREFIX_HEADER_IGNORE_PATH "${ROOT_DIR}/dependencies/nuklear;${ROOT_DIR}/src;${ROOT_DIR}/dependencies/moodycamel/concurrentqueue.h")
		set_property(GLOBAL PROPERTY COTIRE_ADD_UNITY_BUILD FALSE)
	endif()
endif()

add_subdirectory(dependencies)
add_subdirectory(src)

