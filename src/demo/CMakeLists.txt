cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_demo LANGUAGES CXX)


include(shaders.cmake)

file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_executable(demo ${HEADER_FILES}
	src/systems/nim_system.cpp
	src/game_engine.cpp
	src/main.cpp
	src/meta_system.cpp
	src/test_screen.cpp
)

target_compile_options(demo PRIVATE -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-unused-private-field -Wno-missing-braces)

target_link_libraries(demo
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
)
add_dependencies(demo demo_shaders)

