function(mirrage_copy_recursive src dst)
	foreach(path ${src})
		get_filename_component(file ${path} NAME)
		if(IS_DIRECTORY ${path})
			file(GLOB files ${path}/*)
			mirrage_copy_recursive("${files}" "${dst}/${file}")
			set(local_copied_files ${local_copied_files} ${copied_files}) 
		else()
			execute_process(
				COMMAND ${CMAKE_COMMAND} -E make_directory "${dst}"
				COMMAND ${CMAKE_COMMAND} -E copy_if_different "${path}" "${dst}/"
				OUTPUT_QUIET
			)
			list(APPEND local_copied_files "${dst}/${file}") 
		endif()
	endforeach(path)
	
	set(copied_files "${local_copied_files}" PARENT_SCOPE)
endfunction()


mirrage_copy_recursive(${SRC_FILES} "${DST_DIR}/embed")

execute_process(COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${DST_DIR}/embedded_assets.zip" --format=zip ${copied_files}
	WORKING_DIRECTORY "${DST_DIR}/embed"
	OUTPUT_QUIET
)
