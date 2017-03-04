cmake_minimum_required(VERSION 2.8.7)
project(SDL_vulkan)

option(DEV_BUILD "Development Build" OFF)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
include_directories(
  ${SDL2_INCLUDE_DIRS}
  ${VULKAN_INCLUDE_DIR})

find_package(X11_XCB)
if(X11_XCB_FOUND)
  add_definitions(-DHAVE_X11_XCB)
  list(APPEND SUBSYSTEMS [X11])
  include_directories(${X11_XCB_INCLUDE_DIRS})
endif()

include (CheckCCompilerFlag)

function(add_checked_flag flag var)
  check_c_compiler_flag(${flag} HAVE_FLAG_${var})
  if(HAVE_FLAG_${var})
    add_compile_options(${flag})
  endif()
endfunction()

if(DEV_BUILD)
  add_checked_flag(-Wall Wall)
  add_checked_flag(-Werror Werror)
endif()

add_checked_flag(-std=c99 C99)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
install(DIRECTORY include/ DESTINATION include)

add_subdirectory(src)

if(DEV_BUILD)
  enable_testing()
  add_subdirectory(test)
endif()

message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
message(STATUS "Development mode: " ${DEV_BUILD})
message(STATUS "Subsystems: " ${SUBSYSTEMS})

