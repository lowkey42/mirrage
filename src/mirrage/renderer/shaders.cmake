# build shaders
file(GLOB_RECURSE GLSL_SOURCE_FILES
	"${MIRRAGE_ROOT_DIR}/assets/core_assets/shader/*.frag"
	"${MIRRAGE_ROOT_DIR}/assets/core_assets/shader/*.vert"
	"${MIRRAGE_ROOT_DIR}/assets/core_assets/shader/*.comp"
)

set(GLSL_COMPILER "glslc")

foreach(GLSL ${GLSL_SOURCE_FILES})
	get_filename_component(FILE_NAME ${GLSL} NAME)
	set(SPIRV "${MIRRAGE_ROOT_DIR}/assets/core_assets/shader/bin/${FILE_NAME}.spv")
	add_custom_command(
	OUTPUT ${SPIRV}
	COMMAND ${CMAKE_COMMAND} -E make_directory "${MIRRAGE_ROOT_DIR}/assets/core_assets/shader/bin/"
	COMMAND ${GLSL_COMPILER} ${GLSL} -o ${SPIRV}
	DEPENDS ${GLSL})
	list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
	demo_shaders 
	DEPENDS ${SPIRV_BINARY_FILES}
)
