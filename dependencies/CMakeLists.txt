cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

execute_process(COMMAND git submodule update --init --recursive --remote
	WORKING_DIRECTORY ${ROOT_DIR})


find_package(assimp 3.3.1)
if(NOT assimp_FOUND)
	add_subdirectory(assimp)
endif()

add_subdirectory(gli)
add_subdirectory(glm)
include(gsl_interface.cmake)
add_subdirectory(moodycamel)
include(nuklear_interface.cmake)

SET(PHYSFS_INTERNAL_ZLIB TRUE)
SET(PHYSFS_ARCHIVE_7Z FALSE)
SET(PHYSFS_BUILD_SHARED FALSE)
add_subdirectory(physfs)

add_subdirectory(sdl_vulkan)
add_subdirectory(sf2)
add_subdirectory(stb_image)

