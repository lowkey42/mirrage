cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_mesh_converter LANGUAGES CXX)


add_executable(mesh_converter
	main.cpp
	model_parser.hpp
	model_parser.cpp
	material_parser.hpp
	material_parser.cpp
	filesystem.hpp
	filesystem.cpp
)

add_backward(mesh_converter)

target_link_libraries(mesh_converter
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
		mirrage::utils
		assimp
		sf2
		stb_image
		doctest
)

