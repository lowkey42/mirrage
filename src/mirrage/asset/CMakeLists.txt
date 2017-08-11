cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_asset LANGUAGES CXX)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_asset STATIC
	src/aid.cpp
	src/asset_manager.cpp
	src/stream.cpp
	${HEADER_FILES}
)
add_library(mirrage::asset ALIAS mirrage_asset)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_asset PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_asset PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_asset
	PUBLIC
		mirrage::utils
		gsl
		physfs-static
)
   

install(TARGETS mirrage_asset EXPORT mirrage_asset_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(
	DIRECTORY ${CMAKE_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)
install(EXPORT mirrage_asset_targets
	FILE mirrage_asset.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)

