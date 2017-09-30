cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_graphic LANGUAGES CXX)

find_package(Vulkan REQUIRED)

find_package(SDL2 REQUIRED)
add_library(sdl2::sdl2 INTERFACE IMPORTED)
set_property(TARGET sdl2::sdl2 PROPERTY INTERFACE_LINK_LIBRARIES ${SDL2_LIBRARY})
set_property(TARGET sdl2::sdl2 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SDL2_INCLUDE_DIR})


# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_graphic STATIC
	src/context.cpp
	src/device.cpp
	src/device_memory.cpp
	src/ktx_parser.cpp
	src/ktx_parser.hpp
	src/mesh.cpp
	src/profiler.cpp
	src/render_pass.cpp
	src/streamed_buffer.cpp
	src/texture.cpp
	src/transfer_manager.cpp
	src/vk_wrapper.cpp
	src/window.cpp
	${HEADER_FILES}
)
add_library(mirrage::graphic ALIAS mirrage_graphic)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_graphic PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_graphic PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_graphic
	PUBLIC
		mirrage::utils
		glm
		gsl
		mirrage::asset
		sf2
		sdl2::sdl2
		SDL_vulkan
		Vulkan::Vulkan
)


install(TARGETS mirrage_graphic EXPORT mirrage_graphic_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(
	DIRECTORY ${CMAKE_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)
install(EXPORT mirrage_graphic_targets
	FILE mirrage_graphic.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)

