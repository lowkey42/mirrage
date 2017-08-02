cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_renderer LANGUAGES CXX)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_renderer STATIC
	src/pass/blit_pass.cpp 
	src/pass/bloom_pass.cpp
	src/pass/deferred_geometry_subpass.cpp
	src/pass/deferred_lighting_subpass.cpp
	src/pass/deferred_pass.cpp
	src/pass/gen_mipmap_pass.cpp
	src/pass/gi_pass.cpp
	src/pass/gui_pass.cpp
	src/pass/shadowmapping_pass.cpp
	src/pass/ssao_pass.cpp
	src/pass/taa_pass.cpp
	src/pass/tone_mapping_pass.cpp
	src/pass/voxelization_pass.cpp
	
	src/camera_comp.cpp
	src/deferred_renderer.cpp
	src/gbuffer.cpp
	src/light_comp.cpp
	src/loading_system.cpp
	src/model_comp.cpp
	src/model.cpp
	
	${HEADER_FILES}
)
add_library(mirrage::renderer ALIAS mirrage_renderer)

target_compile_options(mirrage_renderer PRIVATE -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-unused-private-field -Wno-missing-braces)


target_include_directories(mirrage_renderer PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_renderer
	PUBLIC
		mirrage::asset
		mirrage::ecs
		mirrage::graphic
		mirrage::gui
		mirrage::utils
		glm
		gsl
		sf2
		vulkan
)


install(TARGETS mirrage_renderer EXPORT mirrage_renderer_targets
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
install(EXPORT mirrage_renderer_targets
	FILE mirrage_renderer.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)
