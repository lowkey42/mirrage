cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(mirrage_utils)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_utils STATIC
	src/command.cpp
	src/console_command.cpp
	src/defer.cpp
	src/log.cpp
	src/md5.cpp
	src/pool.cpp
	src/purgatory.cpp
	src/reflection.cpp
	src/small_vector.cpp
	src/types.cpp
	${HEADER_FILES}
)
add_library(mirrage::utils ALIAS mirrage_utils)
target_compile_features(mirrage_utils PUBLIC cxx_std_17)

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

if(${MIRRAGE_ENABLE_BACKWARD})
	add_backward(mirrage_utils)
	target_compile_definitions(mirrage_utils PUBLIC MIRRAGE_ENABLE_BACKWARD)
endif()

target_link_libraries(mirrage_utils
	PUBLIC
		gsl
		glm::glm
		moodycamel
		sf2
		plog
		doctest
)

if(MIRRAGE_ENABLE_TESTS)
	file(WRITE "${PROJECT_BINARY_DIR}/generated_test.cpp" "#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN\n#include <doctest.h>\n\n")
	foreach(file ${HEADER_FILES})
		STRING(REGEX REPLACE "^include/" "" file_include_path ${file})
		file(APPEND "${PROJECT_BINARY_DIR}/generated_test.cpp" "#include <${file_include_path}>\n")
	endforeach(file)

	add_executable(mirrage_utils_tests
		generated_test.cpp
		test/random_uuid_generator.test.cpp
	)
	target_link_libraries(mirrage_utils_tests doctest mirrage_utils)
	
	if(${MIRRAGE_ENABLE_BACKWARD})
		add_backward(mirrage_utils_tests)
	endif()

	add_test (NAME mirrage_utils_tests COMMAND mirrage_utils_tests)
endif(MIRRAGE_ENABLE_TESTS)

install(TARGETS mirrage_utils EXPORT mirrage_utils_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_utils_targets
	FILE mirrage_utils.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage_utils)
endif()
