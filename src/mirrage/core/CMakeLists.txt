cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage)

include(version_info.cmake)

find_package(SDL2 REQUIRED)
add_library(sdl2::sdl2 INTERFACE IMPORTED)
set_property(TARGET sdl2::sdl2 PROPERTY INTERFACE_LINK_LIBRARIES ${SDL2_LIBRARY})
set_property(TARGET sdl2::sdl2 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SDL2_INCLUDE_DIR})


# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage STATIC 
	${CMAKE_CURRENT_BINARY_DIR}/info.cpp
	src/engine.cpp
	src/screen.cpp
	src/translation.cpp
	${HEADER_FILES}
)
add_library(mirrage::mirrage ALIAS mirrage)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets_mirrage_core.map" CONTENT
"cfg:input_mapping = settings/input_mapping.json
cfg:graphics = graphics-cfg.json
cfg:renderer = renderer-cfg.json
cfg:sounds = sounds-cfg.json
cfg:language = language-cfg.json
cfg:languages_info = settings/languages.json
cfg:gui = settings/gui.json

loc: = loc/
pl_cache: = pipeline_caches
")
mirrage_embed_asset(mirrage "${CMAKE_CURRENT_BINARY_DIR}/assets_mirrage_core.map")

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage
	PUBLIC
		mirrage::utils
		sdl2::sdl2
		mirrage::asset
		mirrage::ecs
		mirrage::graphic
		mirrage::gui
		mirrage::input
		mirrage::net
		glm::glm
)


install(TARGETS mirrage EXPORT mirrage_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_targets
	FILE mirrage.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage)
endif()
