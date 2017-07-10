cmake_minimum_required(VERSION 2.6)

project(stb_image)

ADD_LIBRARY(stb_image STATIC stb_image.c)
target_include_directories(stb_image INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>)

install(TARGETS stb_image EXPORT stb_imageTargets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

export(
	EXPORT stb_imageTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/stb_imageTargets.cmake"
)

install(
	EXPORT stb_imageTargets FILE stb_imageTargets.cmake
	NAMESPACE stb_image::
	DESTINATION lib/cmake
)
