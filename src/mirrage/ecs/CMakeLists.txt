cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(mirrage_ecs)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_ecs STATIC
	src/components/transform_comp.cpp
	src/component.cpp
	src/entity_manager.cpp
	src/entity_handle.cpp
	src/serializer.cpp
	src/types.cpp
	${HEADER_FILES}
)
add_library(mirrage::ecs ALIAS mirrage_ecs)
target_compile_features(mirrage_ecs PUBLIC cxx_std_17)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_ecs PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_ecs PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_ecs
	PUBLIC
		mirrage::utils
		glm::glm
		gsl
		mirrage::asset
		robin-map
		moodycamel
		sf2
)
   

install(TARGETS mirrage_ecs EXPORT mirrage_ecs_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_ecs_targets
	FILE mirrage_ecs.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage_ecs)
endif()
