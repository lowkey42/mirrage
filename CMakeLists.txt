cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# required at top-level
set(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${ROOT_DIR}/modules")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024 -fno-strict-aliasing")

	option(MIRRAGE_ENABLE_LTO "Enable link-time optimization" OFF)
	if(MIRRAGE_ENABLE_LTO)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
	endif()
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	option(MIRRAGE_USE_LIBCPP "Uses libc++ instead of libstdc++. " ON)
	if(MIRRAGE_USE_LIBCPP)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
	endif()
endif()


option(MIRRAGE_BUILD_MESH_CONVERTER "Build the mesh converter (requires assimp)" OFF)

add_subdirectory(dependencies)
add_subdirectory(src)



# custom target that does nothing but references all files so they show up in QtCreator
file(GLOB_RECURSE ALL_FILES
		 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		 ${CMAKE_CURRENT_SOURCE_DIR}/*.*)

add_custom_target(zz_all_project_files SOURCES ${ALL_FILES})

