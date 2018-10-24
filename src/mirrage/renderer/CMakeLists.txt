cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_renderer)

#shaders
file(GLOB_RECURSE GLSL_SOURCE_FILES
	shader/*.frag
	shader/*.vert
	shader/*.comp
)

file(GLOB_RECURSE ALL_SHADERS
		 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	shader/*.frag
	shader/*.vert
	shader/*.comp
	shader/*.glsl)

add_custom_target(mirrage_renderer_shaders SOURCES ${ALL_SHADERS})


foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/shader/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shader/"
    COMMAND ${CMAKE_GLSL_COMPILER} -o ${SPIRV} -c ${GLSL}
    DEPENDS ${ALL_SHADERS})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)


# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_renderer STATIC
	src/pass/animation_pass.cpp
	src/pass/blit_pass.cpp 
	src/pass/bloom_pass.cpp
	src/pass/debug_draw_pass.cpp
	src/pass/deferred_geometry_subpass.cpp
	src/pass/deferred_lighting_subpass.cpp
	src/pass/deferred_pass.cpp
	src/pass/frustum_culling_pass.cpp
	src/pass/gen_mipmap_pass.cpp
	src/pass/gi_pass.cpp
	src/pass/gui_pass.cpp
	src/pass/shadowmapping_pass.cpp
	src/pass/ssao_pass.cpp
	src/pass/taa_pass.cpp
	src/pass/tone_mapping_pass.cpp
	
	src/animation.cpp
	src/animation_comp.cpp
	src/camera_comp.cpp
	src/deferred_renderer.cpp
	src/gbuffer.cpp
	src/light_comp.cpp
	src/loading_system.cpp
	src/model_comp.cpp
	src/model.cpp
	src/particle_system.cpp
	src/render_pass.cpp
	
	${HEADER_FILES}
)
add_library(mirrage::renderer ALIAS mirrage_renderer)
mirrage_embed_asset(mirrage_renderer "${CMAKE_CURRENT_BINARY_DIR}/shader" ${SPIRV_BINARY_FILES})

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_renderer PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})

target_include_directories(mirrage_renderer PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_renderer
	PUBLIC
		mirrage::mirrage
		mirrage::asset
		mirrage::ecs
		mirrage::graphic
		mirrage::gui
		mirrage::utils
		glm::glm
		gsl
		sf2
		robin-map
)

install(TARGETS mirrage_renderer EXPORT mirrage_renderer_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_renderer_targets
	FILE mirrage_renderer.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage_renderer)
endif()
