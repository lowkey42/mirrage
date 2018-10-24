# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# Automatically find the a suitable GLSL to SPIR-V compiler.
# By default it will look for a compiler as named by the CMAKE_GLSL_COMPILER_LIST
# variable.  It can be overridden by setting:
#
# - CMAKE_GLSL_COMPILER: Name or full path of the GLSL compiler.
#      e.g. my-glslc   # Expected on the path
#      e.g. /home/self/my-glslc
#
# - otherwise environment variable GLSL_COMPILER: Name or full path of the GLSL compiler
#      e.g. my-glslc   # Expected on the path
#      e.g. /home/self/my-glslc
#

include(${CMAKE_ROOT}/Modules/CMakeDetermineCompiler.cmake)

set(CMAKE_GLSL_COMPILER_ENV_VAR "GLSL_COMPILER")

# Name of GLSL compilers expected to be on the path.
set(CMAKE_GLSL_COMPILER_LIST glslc)

if(NOT CMAKE_GLSL_COMPILER)
  set(CMAKE_GLSL_COMPILER_INIT NOTFOUND)

  # Prefer the environment variable GLSL_COMPILER, and set
  # CMAKE_GLSL_COMPILER_INIT if we find it.
  if(NOT $ENV{GLSL_COMPILER} STREQUAL "")
    get_filename_component(CMAKE_GLSL_COMPILER_INIT $ENV{GLSL_COMPILER} PROGRAM PROGRAM_ARGS CMAKE_GLSL_FLAGS_ENV_INIT)
    if(CMAKE_GLSL_FLAGS_ENV_INIT)
      set(CMAKE_GLSL_COMPILER_ARG1 "${CMAKE_GLSL_FLAGS_ENV_INIT}" CACHE STRING "First argument to GLSL compiler")
    endif()
    if(NOT EXISTS ${CMAKE_GLSL_COMPILER_INIT})
      message(FATAL_ERROR "Could not find compiler set in environment variable GLSL_COMPILER:\n$ENV{GLSL_COMPILER}\n${CMAKE_GLSL_COMPILER_INIT}")
    endif()
  endif()

  # Try finding it, or fall back to compilers named by CMAKE_GLSL_COMPILER_LIST
  _cmake_find_compiler(GLSL)
else()
  # Fall back to compilers named by CMAKE_GLSL_COMPILER_LIST
  _cmake_find_compiler_path(GLSL)
endif()
mark_as_advanced(CMAKE_GLSL_COMPILER)

# configure variables set in this file for fast reload later on
configure_file(${CMAKE_CURRENT_LIST_DIR}/CMakeGLSLCompiler.cmake.in
  ${CMAKE_PLATFORM_INFO_DIR}/CMakeGLSLCompiler.cmake
  @ONLY
  )

