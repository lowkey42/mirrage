cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(mirrage_error)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_error STATIC
	src/error.cpp
	${HEADER_FILES}
)
add_library(mirrage::error ALIAS mirrage_error)
target_compile_features(mirrage_error PUBLIC cxx_std_17)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_error PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_error PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_error
	PUBLIC
		mirrage::utils
)

if(MIRRAGE_ENABLE_PCH)
	target_link_libraries(mirrage_error PRIVATE mirrage::pch)
	target_precompile_headers(mirrage_error REUSE_FROM mirrage::pch)
endif()


install(TARGETS mirrage_error EXPORT mirrage_error_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_error_targets
	FILE mirrage_error.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)

