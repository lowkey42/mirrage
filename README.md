[![Mirrage logo](/logo.svg)]()

## Mirrage

Mirrage (Mirrage Indirect Radiance Renderer And Game Engine) is a Vulkan based deferred renderer with bits and pieces of a simple game engine, that has been developed as part of my CS Bachelor thesis about screen-space global illumination. As such it is (at least in its current state) mostly just a fancy renderer with a simple demo application and just enough engine stuff (ECS, input, ui, glue-code) to keep that running. But in the future I will hopefully get it to an actually usefull state and use it as a basis for my future projects.


### Demo
<a href="http://www.youtube.com/watch?feature=player_embedded&v=e1NXM5U4Rig" target="_blank"><img src="screenshots/video_thumbnail.jpeg" alt="Demo Video" height="180" border="10" /></a>

|------------------|------------------|
| ![](screenshots/top_down.jpeg) | ![](screenshots/hallway_1.jpeg) |
| ![](screenshots/metal.jpeg) | ![](screenshots/cornell.jpeg) |
| ![](screenshots/hallway_2.jpeg) | ![](screenshots/light_cube.jpeg) |
|------------------|------------------|


### Dependencies
Required:
- CMake >= 3.8
- SDL2 >= 2.0.5
- Vulkan >= 1.0.59


Included in this repository:
- Assimp 3.3.1 (only for mesh-converter)
- glm
- gsl
- moodycamel
- nuklear
- physicsFS
- SF2
- stb_image (only for mesh-converter)


### Supported Compilers
- GCC >= 7
- Clang >= 5


### Build from Source
- git clone FOO
- mkdir FOO_build
- cd FOO_build
- cmake ../FOO
- cmake --build .

The active features of the project can be modified by setting the following CMake-Properties (-DPROP=ON/OFF):
- MIRRAGE_BUILD_MESH_CONVERTER: Also build the mesh converter that can be used to converter models into the engine specific data format (Default: OFF)
- MIRRAGE_ENABLE_CLANG_FORMAT: Includes an additional clangformat target, that can be used to automatically format all source files in the project
- MIRRAGE_ENABLE_LTO: Activates link time optimizations on gcc/clang (Default: OFF)
- MIRRAGE_SAN: Build with clang sanatizers (address, integer, undefined and address-use-after-scope) (Default: OFF)
- MIRRAGE_USE_LIBCPP: Uses libc++ instead of libstdc++ when compiling with clang (Default: ON)
