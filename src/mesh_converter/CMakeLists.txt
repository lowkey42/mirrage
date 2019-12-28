cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(mirrage_mesh_converter LANGUAGES CXX)

add_executable(mesh_converter
	animation_parser.cpp
	animation_parser.hpp
	common.cpp
	common.hpp
	filesystem.cpp
	filesystem.hpp
	main.cpp
	material_parser.cpp
	material_parser.hpp
	model_parser.cpp
	model_parser.hpp
	skeleton_parser.cpp
	skeleton_parser.hpp
	helper/progress.hpp
	helper/progress.cpp
	helper/console.hpp
	${BACKWARD_ENABLE}
)
if(${MIRRAGE_ENABLE_BACKWARD})
	add_backward(mesh_converter)
endif()
target_compile_features(mesh_converter PUBLIC cxx_std_17)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -Ofast")
endif()

target_link_libraries(mesh_converter
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
		mirrage::utils
		assimp
		sf2
		stb_image
		doctest
		crunch::crunch
		Async++
		cxxopts::cxxopts
		indica::indica
)
