cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(imgui)

add_library(imgui STATIC
	imgui/imgui.cpp
	imgui/imgui.h
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_internal.h
	imgui/imgui_widgets.cpp
	imgui/imstb_rectpack.h
	imgui/imstb_textedit.h
	imgui/imstb_truetype.h
)
add_library(imgui::imgui ALIAS imgui)
target_include_directories(imgui SYSTEM INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui>
	$<INSTALL_INTERFACE:include>
)
install(TARGETS imgui EXPORT imguiTargets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

export(
	EXPORT imguiTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/imguiTargets.cmake"
)

install(
	EXPORT imguiTargets FILE imguiTargets.cmake
	NAMESPACE imgui::
	DESTINATION lib/cmake
)
