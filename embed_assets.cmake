cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

#optional: generated files to depend on
macro(mirrage_embed_asset target src_files)
	string (REPLACE ";" "$<SEMICOLON>" src_files_str "${src_files}")

	set(ID "mirrage_embedded_asset_${target}")
	if(MSVC)
		set(EMBED_SRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.rc")
		set(EMBED_MODE "MSVC")
	elseif(APPLE)
		set(EMBED_SRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s")
		set(EMBED_MODE "APPLE")
	else()
		set(EMBED_SRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s")
		set(EMBED_MODE "ASM")
	endif()
	
	add_custom_command(OUTPUT ${EMBED_SRC_FILE}
		COMMAND ${CMAKE_COMMAND} -DMIRRAGE_ROOT_DIR=${MIRRAGE_ROOT_DIR} -DEMBED_MODE=${EMBED_MODE} -DID=${ID} -DSRC_FILES=${src_files_str} -DDST_DIR=${CMAKE_CURRENT_BINARY_DIR} -P ${MIRRAGE_ROOT_DIR}/embed_recursive_into_asm.cmake
		DEPENDS ${ARGN}
		VERBATIM
	)

	set(PLATFORM_EMBED_SRC "")

	if(MSVC)
		string(TOUPPER "${ID}" RES_ID)
		set(PLATFORM_EMBED_SRC
"#include <windows.h>

static auto create_asset() -> mirrage::asset::Embedded_asset {
	auto handle = GetModuleHandle(NULL);
	auto res = FindResource(handle, \"${RES_ID}\", RT_RCDATA);

	return mirrage::asset::Embedded_asset(\"${target}\",
		gsl::span<const gsl::byte>{reinterpret_cast<const gsl::byte*>(LockResource(LoadResource(handle, res))),
		                           static_cast<int>(SizeofResource(handle, res))});
}

static auto mirrage_asset_reg = create_asset();
")

	else()
		set(PLATFORM_EMBED_SRC 
"extern \"C\" const char ${ID}[];
extern \"C\" const int ${ID}_size;

static auto mirrage_asset_reg = mirrage::asset::Embedded_asset(\"${target}\",
		gsl::span<const gsl::byte>{reinterpret_cast<const gsl::byte*>(&*${ID}), ${ID}_size});
")
	endif()

	file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp" CONTENT
"#include <mirrage/asset/embedded_asset.hpp>

${PLATFORM_EMBED_SRC}

void ref_embedded_assets_${target}() {
	volatile mirrage::asset::Embedded_asset* x = &mirrage_asset_reg;
#ifndef _MSC_VER
	asm volatile(\"\" : \"+r\" (x));
#endif
}
")

	target_sources(${target} PRIVATE ${EMBED_SRC_FILE})
	target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp")
	add_custom_target(mirrage_embedded_assets_${target} DEPENDS ${EMBED_SRC_FILE})
	add_dependencies(${target} mirrage_embedded_assets_${target})
endmacro()

