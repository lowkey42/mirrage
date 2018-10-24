cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

add_subdirectory(mirrage)

if(MIRRAGE_BUILD_DEMO)
	add_subdirectory(demo)
endif()

if(MIRRAGE_BUILD_MESH_CONVERTER)
	add_subdirectory(mesh_converter)
endif()

