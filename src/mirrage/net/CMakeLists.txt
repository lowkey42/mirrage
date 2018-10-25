cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_net)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_net STATIC
	src/channel.cpp
	src/client.cpp
	src/common.cpp
	src/error.cpp
	src/message_bridge.cpp
	src/net_manager.cpp
	src/server.cpp
	${HEADER_FILES}
)
add_library(mirrage::net ALIAS mirrage_net)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(mirrage_net PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})


target_include_directories(mirrage_net PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_net
	PUBLIC
		mirrage::utils
		glm::glm
		mirrage::error
		enet
		boost::magic_get
)


install(TARGETS mirrage_net EXPORT mirrage_net_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT mirrage_net_targets
	FILE mirrage_net.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)


if(COMMAND cotire)
	cotire(mirrage_net)
endif()
