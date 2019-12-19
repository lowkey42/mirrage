cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(mirrage_audio)

file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_audio STATIC
	src/audio_manager.cpp
	src/sound_bank.cpp
	src/audio_source_comp.cpp
	src/sound_effect_system.cpp
	src/listener_comp.cpp

	${HEADER_FILES}
)
add_library(mirrage::audio ALIAS mirrage_audio)
target_compile_features(mirrage_audio PUBLIC cxx_std_17)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_audio PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})

target_include_directories(mirrage_audio PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_audio
	PUBLIC
		mirrage::mirrage
		mirrage::asset
		mirrage::ecs
		mirrage::utils
		soloud::soloud
		glm::glm
		gsl
		sf2
		robin-map
)

install(TARGETS mirrage_audio EXPORT mirrage_audio_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_audio_targets
	FILE mirrage_audio.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage_audio)
endif()
