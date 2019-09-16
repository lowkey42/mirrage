cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

function(mirrage_copy_recursive src dst)
	foreach(path ${src})
		get_filename_component(file ${path} NAME)
		if(IS_DIRECTORY ${path})
			set(files "")
			file(GLOB files ${path}/*)
			mirrage_copy_recursive("${files}" "${dst}/${file}")
			set(local_copied_files ${local_copied_files} ${copied_files}) 
		else()
			execute_process(
				COMMAND ${CMAKE_COMMAND} -E make_directory "${dst}"
				OUTPUT_QUIET
			)
			execute_process(
				COMMAND ${CMAKE_COMMAND} -E copy_if_different "${path}" "${dst}/"
				OUTPUT_QUIET
			)
			file(RELATIVE_PATH REL_PATH "${DST_DIR}/embed" "${dst}/${file}")
			list(APPEND local_copied_files ${REL_PATH}) 
		endif()
	endforeach(path)
	
	set(copied_files "${local_copied_files}" PARENT_SCOPE)
endfunction()

mirrage_copy_recursive("${SRC_FILES}" "${DST_DIR}/embed")

execute_process(COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${DST_DIR}/embedded_assets.zip" --format=zip ${copied_files}
	WORKING_DIRECTORY "${DST_DIR}/embed"
	OUTPUT_QUIET
)

if("${EMBED_MODE} " STREQUAL "MSVC ")
	string(TOUPPER "${ID}" RES_ID)
	configure_file(${MIRRAGE_ROOT_DIR}/embedded_assets.rc.in "${EMBED_SRC_FILE}")
	execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${EMBED_SRC_FILE}" OUTPUT_QUIET)
else()
	if("${EMBED_MODE} " STREQUAL "APPLE ")
		set(SECTION ".const_data, no_dead_strip")
	else()
		set(SECTION ".section .rodata")
	endif()
	configure_file(${MIRRAGE_ROOT_DIR}/embedded_assets.s.in "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s")
	execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.s" OUTPUT_QUIET)
endif()
