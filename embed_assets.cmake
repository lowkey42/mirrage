cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

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

extern const char ${ID}[];
extern int ${ID}_size;

static auto mirrage_asset_reg = mirrage::asset::Embedded_asset(\"${target}\",
		gsl::span<const gsl::byte>{reinterpret_cast<const gsl::byte*>(&*${ID}), ${ID}_size});

void ref_embedded_assets_${target}() {
	volatile auto x = &mirrage_asset_reg;
	asm volatile(\"\" : \"+r\" (x));
}
")
	
	target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s")
	target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp")
	add_custom_target(mirrage_embedded_assets_${target} DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.zip" "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s") 
	add_dependencies(${target} mirrage_embedded_assets_${target})
endmacro()

