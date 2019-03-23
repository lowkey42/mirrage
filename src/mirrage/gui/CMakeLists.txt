cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(mirrage_gui)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_gui STATIC
	src/color_picker.cpp
	src/debug_ui.cpp
	src/gui.cpp
	src/menu.cpp
	src/text_edit.cpp
	${HEADER_FILES}
)
add_library(mirrage::gui ALIAS mirrage_gui)
target_compile_features(mirrage_gui PUBLIC cxx_std_17)

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets_mirrage_gui.map" CONTENT
"cfg:gui = settings/gui.json
cfg:console_history = console_history.txt
font: = fonts/*.ttf
font:default_font = fonts/default_font.ttf
")
mirrage_embed_asset(TARGET mirrage_gui
					EXPORT mirrage_gui_targets
					SOURCES
						"${CMAKE_CURRENT_BINARY_DIR}/assets_mirrage_gui.map"
						"${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
					DEPENDS
						"${CMAKE_CURRENT_BINARY_DIR}/assets_mirrage_gui.map")

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_gui PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_gui PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_gui
	PUBLIC
		mirrage::utils
		gsl
		glm::glm
		nuklear
		mirrage::asset
		mirrage::input
		sf2
)
   

install(TARGETS mirrage_gui EXPORT mirrage_gui_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_gui_targets
	FILE mirrage_gui.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage_gui)
endif()
