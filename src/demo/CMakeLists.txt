cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(mirrage_demo LANGUAGES CXX ASM)

file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_executable(demo ${HEADER_FILES}
	src/systems/nim_system.cpp
	src/game_engine.cpp
	src/main.cpp
	src/meta_system.cpp
	src/test_screen.cpp
	src/test_animation_screen.cpp
	${BACKWARD_ENABLE}
)
if(${MIRRAGE_ENABLE_BACKWARD})
	add_backward(demo)
endif()
target_compile_features(demo PUBLIC cxx_std_17)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(demo PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})

target_link_libraries(demo
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
		plog
		glm::glm
		sf2
		doctest
)
if (WIN32)
	if(MINGW)
		target_link_libraries(demo
			PRIVATE
				mingw32
		)
	endif()
	target_link_libraries(demo
		PRIVATE
			SDL2main
	)
endif()

option(MIRRAGE_EXPORT_EXECUTABLE "Export executable" OFF)
if(MIRRAGE_EXPORT_EXECUTABLE)
	install(DIRECTORY ${MIRRAGE_ROOT_DIR}/assets/ DESTINATION bin)
	install(TARGETS demo RUNTIME DESTINATION bin)
endif()

if(COMMAND cotire)
	cotire(demo)
endif()

if(MSVC_IDE)
	set_property(TARGET demo PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${MIRRAGE_ROOT_DIR}/assets")
endif()
