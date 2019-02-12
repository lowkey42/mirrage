cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

SET(BUILD_SHARED_LIBS FALSE CACHE BOOL "" FORCE)
add_subdirectory(asyncplusplus)

if(${MIRRAGE_ENABLE_BACKWARD})
	add_subdirectory(backward-cpp)
endif()

add_subdirectory(enet)
include(glm_interface.cmake)
include(gsl_interface.cmake)
include(moodycamel_interface.cmake)
include(nuklear_interface.cmake)

include(magic_get_interface.cmake)

SET(PHYSFS_ARCHIVE_7Z FALSE CACHE BOOL "" FORCE)
SET(PHYSFS_BUILD_SHARED FALSE CACHE BOOL "" FORCE)
add_subdirectory(physfs)


include(plog_interface.cmake)
include(robin-map_interface.cmake)
include(doctest_interface.cmake)

if (WIN32)
	option(MIRRAGE_PREFER_SYSTEM_SDL "Try to find an already installed SDL2 on the system before building our own" OFF)
else()
	option(MIRRAGE_PREFER_SYSTEM_SDL "Try to find an already installed SDL2 on the system before building our own" ON)
endif()
if(MIRRAGE_PREFER_SYSTEM_SDL)
	find_package(SDL2)
	if(SDL2_FOUND)
		if(TARGET SDL2::SDL2)
			set_target_properties(SDL2::SDL2 PROPERTIES IMPORTED_GLOBAL TRUE)
			set_target_properties(SDL2::SDL2main PROPERTIES IMPORTED_GLOBAL TRUE)
			add_library(mirrage::deps::SDL2 ALIAS SDL2::SDL2)
			add_library(mirrage::deps::SDL2main ALIAS SDL2::SDL2main)
		else()
			add_library(mirrage::deps::SDL2 INTERFACE IMPORTED GLOBAL)
			set_property(TARGET mirrage::deps::SDL2 PROPERTY INTERFACE_LINK_LIBRARIES ${SDL2_LIBRARY})
			set_property(TARGET mirrage::deps::SDL2 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SDL2_INCLUDE_DIR})
		endif()
	endif()
endif()

if(NOT TARGET mirrage::deps::SDL2)
	add_subdirectory(SDL)
	add_library(mirrage::deps::SDL2 ALIAS SDL2-static)
	add_library(mirrage::deps::SDL2main ALIAS SDL2main)
endif()

add_subdirectory(sf2)


if(MIRRAGE_BUILD_MESH_CONVERTER)
	find_package(assimp 4.1)
	if(NOT assimp_FOUND)
		add_subdirectory(assimp)
		set_property(TARGET assimp PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include" "${CMAKE_CURRENT_BINARY_DIR}/assimp/include")
	endif()

	add_subdirectory(stb_image)
endif()
