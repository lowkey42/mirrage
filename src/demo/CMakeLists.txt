cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_demo LANGUAGES CXX)

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

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(demo PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})

target_link_libraries(demo
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
)

option(CompileShaders "Compile demo shaders" ON)
if(CompileShaders)
	include(shaders.cmake)
	add_dependencies(demo demo_shaders)
endif()
