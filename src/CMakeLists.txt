cmake_minimum_required(VERSION 2.6)

# include core
add_subdirectory(core)

project(bachelor_project)

set(VERSION_NAME "0.1")

option(DO_LTO "Enables LTO" OFF)
if(DO_LTO)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
endif()

include_directories(${ROOT_DIR}/src)


option(SAN "Build with sanitizers" OFF)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	MESSAGE("Debug build; Compiler=${CMAKE_CXX_COMPILER_ID}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND SAN)
		MESSAGE("Building with sanitizers")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=integer -fsanitize=undefined -fsanitize-address-use-after-scope ")

	elseif(WIN32)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og")
	endif()

else()
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -gline-tables-only")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
	endif()
endif()


if(WIN32)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lmingw32 -limagehlp -lSDL2main -lglew32 -lws2_32 -lwsock32")
	add_definitions(-DWIN)

else()
	FIND_PACKAGE(ZLIB)
	INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
endif()



if(NOT GIT_EXECUTABLE)
    set(GIT_EXECUTABLE "git")
endif()

# the commit's SHA1, and whether the building workspace was dirty or not
execute_process(COMMAND
  "${GIT_EXECUTABLE}" describe --match=NeVeRmAtCh --always --abbrev=40 --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_HASH
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the date of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_DATE
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the subject of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%s
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_SUBJECT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# generate version.cc
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/info.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/info.cpp" @ONLY)


file(GLOB_RECURSE PROJECT_SRCS
         RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		 game/*.cpp
		 game/*.h*)

add_executable(BachelorProject main.cpp ${PROJECT_SRCS} "${CMAKE_CURRENT_BINARY_DIR}/info.cpp")

target_link_libraries(BachelorProject core)


# build shaders
file(GLOB_RECURSE GLSL_SOURCE_FILES
"${ROOT_DIR}/assets/assets/shader/*.frag"
"${ROOT_DIR}/assets/assets/shader/*.vert"
)

set(GLSL_COMPILER "glslc")

foreach(GLSL ${GLSL_SOURCE_FILES})
get_filename_component(FILE_NAME ${GLSL} NAME)
set(SPIRV "${ROOT_DIR}/assets/assets/shader/bin/${FILE_NAME}.spv")
add_custom_command(
OUTPUT ${SPIRV}
COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT_DIR}/assets/assets/shader/bin/"
COMMAND ${GLSL_COMPILER} ${GLSL} -o ${SPIRV}
DEPENDS ${GLSL})
list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
Shaders 
DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(BachelorProject Shaders)

