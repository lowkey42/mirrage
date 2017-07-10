cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_ecs LANGUAGES CXX)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_ecs STATIC
	src/components/transform_comp.cpp
	src/component.cpp
	src/ecs.cpp
	src/entity_handle.cpp
	src/serializer.cpp
	src/types.cpp
	${HEADER_FILES}
)
add_library(mirrage::ecs ALIAS mirrage_ecs)

target_compile_options(mirrage_ecs PRIVATE -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-unused-private-field -Wno-missing-braces)


target_include_directories(mirrage_ecs PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_ecs
	PUBLIC
		mirrage::utils
		gsl
		mirrage::asset
		moodycamel
		sf2
)
   

install(TARGETS mirrage_ecs EXPORT mirrage_ecs_targets
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
install(EXPORT mirrage_ecs_targets
	FILE mirrage_ecs.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)

