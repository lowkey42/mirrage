cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(mirrage_graphic)

find_package(Vulkan REQUIRED)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_graphic STATIC
	src/context.cpp
	src/descriptor_sets.cpp
	src/device.cpp
	src/device_memory.cpp
	src/ktx_parser.cpp
	src/ktx_parser.hpp
	src/mesh.cpp
	src/pipeline_cache.cpp
	src/profiler.cpp
	src/render_pass.cpp
	src/streamed_buffer.cpp
	src/swapchain.cpp
	src/settings.cpp
	src/texture.cpp
	src/thread_local_command_buffer_pool.cpp
	src/transfer_manager.cpp
	src/vk_wrapper.cpp
	src/window.cpp
	${HEADER_FILES}
)
add_library(mirrage::graphic ALIAS mirrage_graphic)
target_compile_features(mirrage_graphic PUBLIC cxx_std_17)

option(MIRRAGE_VULKAN_USE_LABELS "Use labels if VK_EXT_DEBUG_UTILS_EXTENSION_NAME is available" ON)
if(NOT MIRRAGE_VULKAN_USE_LABELS)
	target_compile_definitions(mirrage_graphic PRIVATE MIRRAGE_IGNORE_VULKAN_LABELS)
endif()

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_graphic PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_graphic PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_graphic
	PUBLIC
		mirrage::utils
		glm::glm
		gsl
		mirrage::asset
		sf2
		mirrage::deps::SDL2
		Vulkan::Vulkan
		stb_image
)

if(MIRRAGE_ENABLE_PCH)
	target_link_libraries(mirrage_graphic PRIVATE mirrage::pch)
	target_precompile_headers(mirrage_graphic REUSE_FROM mirrage::pch)
endif()


install(TARGETS mirrage_graphic EXPORT mirrage_graphic_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_graphic_targets
	FILE mirrage_graphic.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)

