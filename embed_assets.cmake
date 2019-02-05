cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

if(MSVC)
	find_program(AUX_ASSEMBLER as)
	message(STATUS "Found AUX_ASSEMBLER: ${AUX_ASSEMBLER}")
endif()

#optional: generated files to depend on
macro(mirrage_embed_asset target src_files)
	string (REPLACE ";" "$<SEMICOLON>" src_files_str "${src_files}")

	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.zip" "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s"
		COMMAND ${CMAKE_COMMAND} -DSRC_FILES=${src_files_str} -DDST_DIR=${CMAKE_CURRENT_BINARY_DIR} -P ${MIRRAGE_ROOT_DIR}/copy_recursive.cmake
		COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s"
		DEPENDS ${ARGN}
		VERBATIM
	)

	set(ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.zip")
	set(ID "mirrage_embedded_asset_${target}")
	file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s" CONTENT 
"    .global ${ID}
	.global ${ID}_size
	.section .rodata
${ID}:
	.incbin \"${ARCHIVE}\"
1:
${ID}_size:
	.int 1b - ${ID}
	
")
	file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp" CONTENT
"#include <mirrage/asset/embedded_asset.hpp>

extern \"C\" const char ${ID}[];
extern \"C\" int ${ID}_size;

static auto mirrage_asset_reg = mirrage::asset::Embedded_asset(\"${target}\",
		gsl::span<const gsl::byte>{reinterpret_cast<const gsl::byte*>(&*${ID}), ${ID}_size});

void ref_embedded_assets_${target}() {
	volatile mirrage::asset::Embedded_asset* x = &mirrage_asset_reg;
#ifndef _MSC_VER
	asm volatile(\"\" : \"+r\" (x));
#endif
}
")

	if(MSVC)
		add_custom_command(	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s.obj" 
							COMMAND ${AUX_ASSEMBLER} "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s" -o "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s.obj"
							MAIN_DEPENDENCY "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s"
							COMMENT "Running AUX_ASSEMBLER ${AUX_ASSEMBLER} for embedded assets of target ${target}."
							VERBATIM)
		target_link_libraries(${target} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s.obj")
	endif()
	target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s")
	target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp")
	add_custom_target(mirrage_embedded_assets_${target} DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.zip" "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s") 
	add_dependencies(${target} mirrage_embedded_assets_${target})
endmacro()

