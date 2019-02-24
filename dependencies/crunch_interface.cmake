cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(crunch)

if(MSVC)
	set(CRUNCH_PLATFORM_SOURCES crunch/crnlib/crn_threading_win32.cpp)
else()
	set(CRUNCH_PLATFORM_SOURCES crunch/crnlib/crn_threading_pthreads.cpp)
endif()


file(GLOB_RECURSE CRUNCH_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/crunch/crnlib/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/crunch/crunch/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/crunch/inc/*.h
)
add_library(crunch STATIC ${CRUNCH_SOURCES} ${CRUNCH_PLATFORM_SOURCES}
	crunch/crnlib/crn_arealist.cpp
	crunch/crnlib/crn_assert.cpp
	crunch/crnlib/crn_checksum.cpp
	crunch/crnlib/crn_colorized_console.cpp
	crunch/crnlib/crn_command_line_params.cpp
	crunch/crnlib/crn_comp.cpp
	crunch/crnlib/crn_console.cpp
	crunch/crnlib/crn_core.cpp
	crunch/crnlib/crn_data_stream.cpp
	crunch/crnlib/crn_dds_comp.cpp
	crunch/crnlib/crn_decomp.cpp
	crunch/crnlib/crn_dxt1.cpp
	crunch/crnlib/crn_dxt5a.cpp
	crunch/crnlib/crn_dxt.cpp
	crunch/crnlib/crn_dxt_endpoint_refiner.cpp
	crunch/crnlib/crn_dxt_fast.cpp
	crunch/crnlib/crn_dxt_hc_common.cpp
	crunch/crnlib/crn_dxt_hc.cpp
	crunch/crnlib/crn_dxt_image.cpp
	crunch/crnlib/crn_dynamic_string.cpp
	crunch/crnlib/crn_etc.cpp
	crunch/crnlib/crn_file_utils.cpp
	crunch/crnlib/crn_find_files.cpp
	crunch/crnlib/crn_hash.cpp
	crunch/crnlib/crn_hash_map.cpp
	crunch/crnlib/crn_huffman_codes.cpp
	crunch/crnlib/crn_image_utils.cpp
	crunch/crnlib/crn_jpgd.cpp
	crunch/crnlib/crn_jpge.cpp
	crunch/crnlib/crn_ktx_texture.cpp
	crunch/crnlib/crnlib.cpp
	crunch/crnlib/crn_lzma_codec.cpp
	crunch/crnlib/crn_math.cpp
	crunch/crnlib/crn_mem.cpp
	crunch/crnlib/crn_miniz.cpp
	crunch/crnlib/crn_mipmapped_texture.cpp
	crunch/crnlib/crn_pixel_format.cpp
	crunch/crnlib/crn_platform.cpp
	crunch/crnlib/crn_prefix_coding.cpp
	crunch/crnlib/crn_qdxt1.cpp
	crunch/crnlib/crn_qdxt5.cpp
	crunch/crnlib/crn_rand.cpp
	crunch/crnlib/crn_resample_filters.cpp
	crunch/crnlib/crn_resampler.cpp
	crunch/crnlib/crn_rg_etc1.cpp
	crunch/crnlib/crn_ryg_dxt.cpp
	crunch/crnlib/crn_sparse_bit_array.cpp
	crunch/crnlib/crn_stb_image.cpp
	crunch/crnlib/crn_strutils.cpp
	crunch/crnlib/crn_symbol_codec.cpp
	crunch/crnlib/crn_texture_comp.cpp
	crunch/crnlib/crn_texture_conversion.cpp
	crunch/crnlib/crn_texture_file_types.cpp
	crunch/crnlib/crn_threaded_resampler.cpp
	crunch/crnlib/crn_timer.cpp
	crunch/crnlib/crn_utils.cpp
	crunch/crnlib/crn_value.cpp
	crunch/crnlib/crn_vector.cpp
	crunch/crnlib/crn_zeng.cpp
	crunch/crnlib/lzma_7zBuf2.cpp
	crunch/crnlib/lzma_7zBuf.cpp
	crunch/crnlib/lzma_7zCrc.cpp
	crunch/crnlib/lzma_7zFile.cpp
	crunch/crnlib/lzma_7zStream.cpp
	crunch/crnlib/lzma_Alloc.cpp
	crunch/crnlib/lzma_Bcj2.cpp
	crunch/crnlib/lzma_Bra86.cpp
	crunch/crnlib/lzma_Bra.cpp
	crunch/crnlib/lzma_BraIA64.cpp
	crunch/crnlib/lzma_LzFind.cpp
	crunch/crnlib/lzma_LzmaDec.cpp
	crunch/crnlib/lzma_LzmaEnc.cpp
	crunch/crnlib/lzma_LzmaLib.cpp
)
add_library(crunch::crunch ALIAS crunch)

if(NOT MSVC)
	target_compile_options(crunch PRIVATE -fno-strict-aliasing -lpthread -w)
endif()

target_include_directories(crunch PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/crunch/inc>
	$<INSTALL_INTERFACE:include>)
	
install(TARGETS crunch EXPORT crunchTargets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)
install(DIRECTORY crunch/inc/ DESTINATION include)

export(
	EXPORT crunchTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/crunchTargets.cmake"
)

install(
	EXPORT crunchTargets FILE crunchTargets.cmake
	DESTINATION lib/cmake
)
