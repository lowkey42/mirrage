cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(mirrage LANGUAGES C CXX)

option(MIRRAGE_BUILD_MESH_CONVERTER "Build the mesh converter (requires assimp)" OFF)

get_directory_property(hasParent PARENT_DIRECTORY)
if(NOT hasParent)
	include(globals.cmake)
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

add_subdirectory(dependencies)
add_subdirectory(src)

if(MSVC_IDE)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT demo)
endif()
