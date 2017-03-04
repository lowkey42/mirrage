function(exe_test name)
  add_executable(${name}_exe ${name}.c)
  target_link_libraries(${name}_exe SDL_vulkan ${SDL2_LIBRARIES} ${VULKAN_LIBRARY})

  add_test(${name} ${name}_exe)
endfunction()

exe_test(no_init_1)
exe_test(no_init_2)
