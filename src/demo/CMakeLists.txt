cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_demo LANGUAGES CXX)


include(shaders.cmake)

# TODO
file(GLOB_RECURSE PROJECT_SRCS
         RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		 ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
		 ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
		 ${CMAKE_CURRENT_SOURCE_DIR}/*.hxx)

add_executable(demo ${PROJECT_SRCS})

target_compile_options(demo PRIVATE -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-unused-private-field -Wno-missing-braces)

target_link_libraries(demo
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
)
add_dependencies(demo demo_shaders)

