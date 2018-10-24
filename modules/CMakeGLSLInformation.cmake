# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

set(CMAKE_GLSL_OUTPUT_EXTENSION .spv)
set(CMAKE_INCLUDE_FLAG_GLSL "-I")

set(CMAKE_GLSL_COMPILE_OBJECT  "<CMAKE_GLSL_COMPILER> <DEFINES> <FLAGS> -o <OBJECT> -c <SOURCE>")

set(CMAKE_GLSL_INFORMATION_LOADED 1)

