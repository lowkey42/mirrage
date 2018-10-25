cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_asset)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_asset STATIC
	src/aid.cpp
	src/asset_manager.cpp
	src/embedded_asset.cpp
	src/error.cpp
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
		mirrage::error
		gsl
		physfs-static
		Async++
		robin-map
		glm::glm
)
   

install(TARGETS mirrage_asset EXPORT mirrage_asset_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_asset_targets
	FILE mirrage_asset.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage_asset)
endif()
