cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

SET(BUILD_SHARED_LIBS FALSE CACHE BOOL "" FORCE)
add_subdirectory(asyncplusplus)

if(${MIRRAGE_ENABLE_BACKWARD})
	add_subdirectory(backward-cpp)
endif()

add_subdirectory(enet)
add_subdirectory(indicators)
include(glm_interface.cmake)
include(gsl_interface.cmake)
include(moodycamel_interface.cmake)
include(imgui_interface.cmake)

if(NOT TARGET boost::pfr)
	include(magic_get_interface.cmake)
endif()

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
			if(NOT EXISTS ${SDL2_INCLUDE_DIR}/SDL.h)
				if(EXISTS ${SDL2_INCLUDE_DIR}/SDL2/SDL.h)
					set(SDL2_INCLUDE_DIR "${SDL2_INCLUDE_DIR}/SDL2")
				else()
					find_path(SDL2_INCLUDE_DIR NAMES SDL_vulkan.h PATH_SUFFIXES SDL2)
					message("Found SDL2 include directory: ${SDL2_INCLUDE_DIR}")
				endif()
			endif()

			if(EXISTS ${SDL2_INCLUDE_DIR}/SDL.h)
				add_library(mirrage::deps::SDL2 INTERFACE IMPORTED GLOBAL)
				set_property(TARGET mirrage::deps::SDL2 PROPERTY INTERFACE_LINK_LIBRARIES ${SDL2_LIBRARIES})
				set_property(TARGET mirrage::deps::SDL2 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SDL2_INCLUDE_DIR})
				message("Generated SDL2 target: ${SDL2_LIBRARIES}; ${SDL2_INCLUDE_DIR}")
			endif()
		endif()
	endif()
endif()

add_subdirectory(sf2)

if(NOT TARGET mirrage::deps::SDL2)
	message("Building SDL2 from source")
	if(MSVC)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4267 /wd4018")
	endif()

	add_subdirectory(SDL)

	export(
		EXPORT SDL2Targets
		FILE "${CMAKE_CURRENT_BINARY_DIR}/SDL2Targets.cmake"
	)
	
	add_library(mirrage::deps::SDL2 ALIAS SDL2-static)
	add_library(mirrage::deps::SDL2main ALIAS SDL2main)
endif()

include(soloud_interface.cmake)

add_subdirectory(stb_image)

if(MIRRAGE_BUILD_MESH_CONVERTER)
	set(CXXOPTS_BUILD_TESTS OFF)
	add_subdirectory(cxxopts)
	add_subdirectory(assimp)
	set_property(TARGET assimp PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include" "${CMAKE_CURRENT_BINARY_DIR}/assimp/include")

	include(crunch_interface.cmake)
endif()

