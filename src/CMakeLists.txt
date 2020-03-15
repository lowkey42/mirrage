cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# target for shared precompiled header
option(MIRRAGE_ENABLE_PCH "Generate and use precompiled headers" ON)
if(MIRRAGE_ENABLE_PCH)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/mirrage_pch_dummy.cpp "")
	add_library(mirrage_pch STATIC ${CMAKE_CURRENT_BINARY_DIR}/mirrage_pch_dummy.cpp)
	add_library(mirrage::pch ALIAS mirrage_pch)

	target_link_libraries(mirrage_pch
		PUBLIC
			gsl
			glm::glm
			moodycamel
			sf2
			plog
			Async++
			robin-map
	)

	set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
	target_compile_options(mirrage_pch PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})

	target_compile_definitions(mirrage_pch PUBLIC GLM_FORCE_RADIANS)
	target_compile_definitions(mirrage_pch PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
	target_compile_definitions(mirrage_pch PUBLIC GLM_ENABLE_EXPERIMENTAL)
	target_compile_definitions(mirrage_pch PUBLIC GLM_FORCE_CXX14)
	target_compile_definitions(mirrage_pch PUBLIC ENABLE_SF2_ASSETS)

	target_precompile_headers(mirrage_pch PRIVATE
		<algorithm>
		<atomic>
		<cassert>
		<chrono>
		<cmath>
		<cstdint>
		<functional>
		<iostream>
		<map>
		<memory>
		<string>
		<tuple>
		<type_traits>
		<unordered_map>
		<utility>
		<vector>
		
		<async++.h>
		<glm/glm.hpp>
		<gsl/gsl>
		<concurrentqueue.h>
		<plog/Log.h>
		<tsl/robin_map.h>
	)
	install(TARGETS mirrage_pch EXPORT mirrage_pch_targets)
	install(EXPORT mirrage_pch_targets
		FILE mirrage_pch.cmake
		NAMESPACE mirrage::
		DESTINATION lib/cmake/mirrage
	)
endif()

# engine libraries
add_subdirectory(mirrage)

# executables
if(MIRRAGE_BUILD_DEMO)
	add_subdirectory(demo)
endif()

if(MIRRAGE_BUILD_MESH_CONVERTER)
	add_subdirectory(mesh_converter)
endif()

