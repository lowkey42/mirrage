# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

if(CMAKE_GLSL_COMPILER_FORCED)
  # The compiler configuration was forced by the user.
  # Assume the user has configured all compiler information.
  set(CMAKE_GLSL_COMPILER_WORKS TRUE)
  return()
endif()

include(CMakeTestCompilerCommon)

# This file is used by EnableLanguage in cmGlobalGenerator to
# determine that that selected GLSL compiler can actually compile
# the most basic of programs.   If not, a fatal error is set and
# cmake stops processing commands and will not generate any makefiles
# or projects.
if(NOT CMAKE_GLSL_COMPILER_WORKS)
  PrintTestCompilerStatus("GLSL" "")
  file(WRITE ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/main.vert
    "#version 150\n"
    "void main() {}\n")
  # Need -c to just make a .spv file
  try_compile(CMAKE_GLSL_COMPILER_WORKS ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/main.vert
    CMAKE_FLAGS -c
    OUTPUT_VARIABLE __CMAKE_GLSL_COMPILER_OUTPUT)
  # Move result from cache to normal variable.
  set(CMAKE_GLSL_COMPILER_WORKS ${CMAKE_GLSL_COMPILER_WORKS})
  unset(CMAKE_GLSL_COMPILER_WORKS CACHE)
  set(GLSL_TEST_WAS_RUN 1)
endif()

if(NOT CMAKE_GLSL_COMPILER_WORKS)
  PrintTestCompilerStatus("GLSL" " -- broken")
  file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
    "Determining if the GLSL compiler works failed with "
    "the following output:\n${__CMAKE_GLSL_COMPILER_OUTPUT}\n\n")
  message(FATAL_ERROR "The GLSL compiler \"${CMAKE_GLSL_COMPILER}\" "
    "is not able to compile a simple test program.\nIt fails "
    "with the following output:\n ${__CMAKE_GLSL_COMPILER_OUTPUT}\n\n"
    "CMake will not be able to correctly generate this project.")
else()
  if(GLSL_TEST_WAS_RUN)
    PrintTestCompilerStatus("GLSL" " -- works")
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "Determining if the GLSL compiler works passed with "
      "the following output:\n${__CMAKE_GLSL_COMPILER_OUTPUT}\n\n")
  endif()
endif()

unset(__CMAKE_GLSL_COMPILER_OUTPUT)

