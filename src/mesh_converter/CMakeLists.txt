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

target_compile_options(mesh_converter PRIVATE -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-unused-private-field -Wno-missing-braces)

target_link_libraries(mesh_converter
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
		mirrage::utils
		assimp
		gli
		sf2
		stb_image
)

