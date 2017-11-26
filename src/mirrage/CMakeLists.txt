cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage LANGUAGES CXX)

add_subdirectory(asset)
add_subdirectory(core)
add_subdirectory(ecs)
add_subdirectory(error)
add_subdirectory(graphic)
add_subdirectory(gui)
add_subdirectory(input)
add_subdirectory(net)
add_subdirectory(renderer)
add_subdirectory(utils)

