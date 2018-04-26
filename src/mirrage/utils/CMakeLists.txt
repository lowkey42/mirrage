cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_utils LANGUAGES CXX)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_utils STATIC
	src/command.cpp
	src/log.cpp
	src/md5.cpp
	src/purgatory.cpp
	src/reflection.cpp
	src/types.cpp
	${HEADER_FILES}
)
add_library(mirrage::utils ALIAS mirrage_utils)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_utils PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})

target_compile_definitions(mirrage_utils PUBLIC GLM_FORCE_RADIANS)
target_compile_definitions(mirrage_utils PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(mirrage_utils PUBLIC GLM_ENABLE_EXPERIMENTAL)
target_compile_definitions(mirrage_utils PUBLIC GLM_FORCE_CXX14)
target_compile_definitions(mirrage_utils PUBLIC ENABLE_SF2_ASSETS)

target_include_directories(mirrage_utils PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

add_backward(mirrage_utils)

target_link_libraries(mirrage_utils
	PUBLIC
		gsl
		glm
		moodycamel
		sf2
		plog
)
   

install(TARGETS mirrage_utils EXPORT mirrage_utils_targets
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
install(EXPORT mirrage_utils_targets
	FILE mirrage_utils.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)

