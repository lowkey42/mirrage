cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(mirrage_input)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_input STATIC
	src/input_manager.cpp
	src/input_mapping.cpp
	${HEADER_FILES}
)
add_library(mirrage::input ALIAS mirrage_input)
target_compile_features(mirrage_input PUBLIC cxx_std_17)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_input PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_input PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_input
	PUBLIC
		mirrage::utils
		gsl
		glm::glm
		mirrage::asset
		SDL2-static
		sf2
)
   

install(TARGETS mirrage_input EXPORT mirrage_input_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_input_targets
	FILE mirrage_input.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage_input)
endif()
