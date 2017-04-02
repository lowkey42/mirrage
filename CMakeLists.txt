cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")

if(WIN32)
	SET(PHYSFS_INTERNAL_ZLIB TRUE)
	SET(PHYSFS_ARCHIVE_7Z FALSE)
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

set(BUILD_SHARED_LIBS OFF)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_BUILD_TESTS OFF)

set(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${ROOT_DIR}/modules")

include_directories(BEFORE "${ROOT_DIR}/dependencies")
include_directories(BEFORE "${ROOT_DIR}/dependencies/gsl/include")
include_directories(BEFORE "${ROOT_DIR}/dependencies/sf2/include")
include_directories(BEFORE "${ROOT_DIR}/dependencies/range-v3/include")
include_directories(BEFORE "${ROOT_DIR}/dependencies/sdl_vulkan/include")
include_directories(BEFORE "${ROOT_DIR}/dependencies/glm")
include_directories(BEFORE "${ROOT_DIR}/dependencies/stb_image")
include_directories(BEFORE SYSTEM "${ROOT_DIR}/dependencies/assimp")
include_directories(BEFORE SYSTEM "${ROOT_DIR}/dependencies/nuklear")
add_definitions(-DGLM_FORCE_RADIANS)
add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
add_definitions(-DENABLE_SF2_ASSETS)
add_definitions(-DSTACKTRACE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-pie")

add_subdirectory(dependencies/physfs)
add_subdirectory(dependencies/stb_image)
add_subdirectory(dependencies/sf2)
add_subdirectory(dependencies/sdl_vulkan)
add_subdirectory(dependencies/assimp)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024 -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-unused-private-field")
add_subdirectory(src)


file(GLOB_RECURSE ALL_FILES
		 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		 ${CMAKE_CURRENT_SOURCE_DIR}/*.*)

add_custom_target(all_project_files SOURCES ${ALL_FILES})
