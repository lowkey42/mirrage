cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(magic_get)

add_library(magic_get INTERFACE)
add_library(boost::magic_get ALIAS magic_get)
target_include_directories(magic_get INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/magic_get/include>
	$<INSTALL_INTERFACE:include>)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	# GCC broke their structured binding implementation in 7.2.0.
	# See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=81888
	# TODO: Remove when fixed.
	target_compile_options(magic_get INTERFACE -DBOOST_PFR_USE_CPP17=0)
endif()

install(TARGETS magic_get EXPORT magic_get_targets INCLUDES DESTINATION include)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/magic_get/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h*")

export(
	EXPORT magic_get_targets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/magic_get_targets.cmake"
)

install(
	EXPORT magic_get_targets FILE magic_get_targets.cmake
	NAMESPACE boost::
	DESTINATION lib/cmake
)
