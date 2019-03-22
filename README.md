[![Mirrage logo](https://github.com/lowkey42/mirrage/raw/develop/logo.svg?sanitize=true)]()

[![pipeline status](https://gitlab.com/lowkey42/mirrage/badges/develop/pipeline.svg)](https://gitlab.com/lowkey42/mirrage/commits/develop)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](/LICENSE)

## Mirrage

Mirrage (Mirrage Indirect Radiance Renderer And Game Engine) is a Vulkan based deferred renderer with bits and pieces of a simple game engine, that has been developed as part of my CS Bachelor thesis about screen-space global illumination. As such it is (at least in its current state) mostly just a fancy renderer with a simple demo application and just enough engine stuff (ECS, input, ui, glue-code) to keep that running. But in the future I will hopefully get it to an actually useful state and use it as a basis for my future projects.

The repository at <a href="https://github.com/lowkey42/mirrage">GitHub</a> is a read-only mirror and the main repository is located on <a href="https://gitlab.com/lowkey42/mirrage">Gitlab.com</a>.

WIP API documentation/examples: <https://gitlab.com/lowkey42/mirrage/wikis/Documentation>

### Demo
<a href="http://www.youtube.com/watch?feature=player_embedded&v=gHHLuwjDiZo" target="_blank"><img src="screenshots/video_thumbnail2.jpeg" alt="Demo Video" height="180" border="10" /></a>
<a href="http://www.youtube.com/watch?feature=player_embedded&v=e1NXM5U4Rig" target="_blank"><img src="screenshots/video_thumbnail.jpeg" alt="Demo Video" height="180" border="10" /></a>

| ![](screenshots/top_down.jpeg) | ![](screenshots/hallway_1.jpeg) |
|------------------|------------------|
| ![](screenshots/metal.jpeg) | ![](screenshots/cornell.jpeg) |
| ![](screenshots/front.jpeg) | ![](screenshots/light_cube.jpeg) |



### Dependencies

Required:
- CMake >= 3.9
- Vulkan + Vulkan-HPP >= 1.1.80
- GLSLC


Included in this repository:
- Assimp 3.3.1 (only for mesh-converter)
- crunch (only for mesh-converter): "Crunch Library Copyright (c) 2010-2016 Richard Geldreich, Jr., Tenacious Software, and Binomial LLC"
- glm
- gsl
- moodycamel
- nuklear
- physicsFS
- SDL2
- SF2
- stb_image (only for mesh-converter)



### Supported Compilers

- GCC >= 8
- Clang >= 7
- MSVC 19.14 (Visual Studio 2017 15.7)



### Build from Source

- git lfs install
- git clone --recurse-submodules https://gitlab.com/lowkey42/mirrage.git
- mkdir mirrage_build
- cd mirrage_build
- cmake ../mirrage
- cmake --build .



The project can be further configured by setting the following CMake-Properties (-DPROP=ON/OFF):
- MIRRAGE_BUILD_DEMO: Build the demo application (Default: ON when building the engine directly, OFF when including it from another CMake-Project)
- MIRRAGE_BUILD_MESH_CONVERTER: Also build the mesh converter that can be used to converter models into the engine specific data format (Default: OFF)
- MIRRAGE_ENABLE_BACKWARD: Enable stacktraces (Default: ON on Linux, OFF on Windows because the underlying library backward-cpp doesn't currently support Windows)
- MIRRAGE_ENABLE_CLANG_FORMAT: Includes an additional clangformat target, that can be used to automatically format all source files in the project
- MIRRAGE_ENABLE_LTO: Activates link time optimizations on gcc/clang (Default: OFF)
- MIRRAGE_SAN: Build with clang sanatizers (address, integer, undefined and address-use-after-scope) (Default: OFF)
- MIRRAGE_ENABLE_COTIRE: Enables automatic precompiled headers through cotire
- MIRRAGE_USE_LIBCPP: Uses libc++ instead of libstdc++ when compiling with clang (Default: ON)
- MIRRAGE_OPTIMIZE_NATIVE: Compile with -march=native (Default: OFF)

