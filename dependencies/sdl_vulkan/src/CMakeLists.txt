add_library(SDL_vulkan STATIC SDL_vulkan.c)
set_target_properties(SDL_vulkan PROPERTIES
  VERSION ${VERSION_MAJOR})
target_link_libraries(SDL_vulkan
  ${SDL2_LIBRARIES}
  Vulkan::Vulkan
  ${X11_XCB_LIBRARIES})

target_include_directories(SDL_vulkan INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
	$<INSTALL_INTERFACE:include>)
install(TARGETS SDL_vulkan EXPORT SDL_vulkan
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
INSTALL(EXPORT SDL_vulkan
	FILE SDL_vulkan.cmake
	DESTINATION lib/cmake
)
