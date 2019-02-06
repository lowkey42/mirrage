cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

if(MSVC)
	find_program(MIRRAGE_AUX_ASSEMBLER as)
	if(MIRRAGE_AUX_ASSEMBLER)
		message(STATUS "Using MIRRAGE_AUX_ASSEMBLER for asset embedding: ${MIRRAGE_AUX_ASSEMBLER}")
	else()
		message(FATAL_ERROR "Can't find a binary for MIRRAGE_AUX_ASSEMBLER. The asset embedding mechanism of mirrage currently doesn't work natively with MSVC. To support MSVC despite this, MinGWs assembler is required to create embedded asset OBJs. However, this binary wasn't found using find_program().")
	endif()
endif()

#optional: generated files to depend on
macro(mirrage_embed_asset target src_files)
	string (REPLACE ";" "$<SEMICOLON>" src_files_str "${src_files}")

	set(ID "mirrage_embedded_asset_${target}")
	
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s"
		COMMAND ${CMAKE_COMMAND} -DMIRRAGE_ROOT_DIR=${MIRRAGE_ROOT_DIR} -DID=${ID} -DSRC_FILES=${src_files_str} -DDST_DIR=${CMAKE_CURRENT_BINARY_DIR} -P ${MIRRAGE_ROOT_DIR}/embed_recursive_into_asm.cmake
		DEPENDS ${ARGN}
		VERBATIM
	)

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
							COMMAND ${MIRRAGE_AUX_ASSEMBLER} "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s" -o "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s.obj"
							MAIN_DEPENDENCY "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s"
							COMMENT "Running MIRRAGE_AUX_ASSEMBLER (${MIRRAGE_AUX_ASSEMBLER}) for embedded assets of target ${target}."
							VERBATIM)
		target_link_libraries(${target} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s.obj")
	endif()
	target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s")
	target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp")
	add_custom_target(mirrage_embedded_assets_${target} DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s") 
	add_dependencies(${target} mirrage_embedded_assets_${target})
endmacro()

