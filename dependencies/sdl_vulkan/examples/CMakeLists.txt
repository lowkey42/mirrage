add_executable(example example.c)
target_link_libraries(example SDL_vulkan ${SDL2_LIBRARIES} ${VULKAN_LIBRARY})
