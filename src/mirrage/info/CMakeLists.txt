cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(mirrage_info)

include(version_info.cmake)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_info STATIC 
	${CMAKE_CURRENT_BINARY_DIR}/info.cpp
	${HEADER_FILES}
)
add_library(mirrage::info ALIAS mirrage_info)
target_compile_features(mirrage_info PUBLIC cxx_std_17)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_info PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_info PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

install(TARGETS mirrage_info EXPORT mirrage_info_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_info_targets
	FILE mirrage_info.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)

