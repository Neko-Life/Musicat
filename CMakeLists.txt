# minimum CMake version required
cmake_minimum_required(VERSION 3.15)
# Project name, version and description
project(Musicat 
	VERSION 0.1.1
	LANGUAGES CXX
	DESCRIPTION "Discord Music Bot written in C++")

option(MUSICAT_WITH_CORO "Configure Musicat with C++20 coroutines" OFF)
option(MUSICAT_DEBUG_SYMBOL "Build Musicat with debug symbol" ON)
option(COMPILE_GNUPLOT "Download and compile gnuplot" OFF)

set(MUSICAT_CXX_STANDARD 17)
set(DPP_INSTALL OFF)
set(DPP_USE_EXTERNAL_JSON ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

set(WITH_COMPILE_GNUPLOT 0)

if (COMPILE_GNUPLOT)
	set(WITH_COMPILE_GNUPLOT 1)
	set(GNUPLOT_VERSION 5.4.9)

	message("-- INFO: Will compile gnuplot version ${GNUPLOT_VERSION}")
endif()

file(GLOB_RECURSE MUSICAT_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/musicat/*.h)

file(GLOB_RECURSE MUSICAT_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/src/musicat/*.cpp)

# Create an executable
add_executable(Shasha
	# include files
	${MUSICAT_HEADER_FILES}
	include/nlohmann/json.hpp
	libs/uWebSockets/uSockets/src/libusockets.h
	libs/uWebSockets/uSockets/src/quic.h
	# !TODO: include/uWebSockets/files

	# impl files
	${MUSICAT_SOURCE_FILES}
	libs/yt-search.h/src/yt-search/encode.cpp
	libs/yt-search.h/src/yt-search/yt-playlist.cpp
	libs/yt-search.h/src/yt-search/yt-search.cpp
	libs/yt-search.h/src/yt-search/yt-track-info.cpp
	libs/nekos-bestpp/src/nekos-best++.cpp
	src/main.cpp)

set(USOCKETS_OBJECT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/bsd.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/context.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/epoll_kqueue.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/gcd.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/libuv.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/loop.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/openssl.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/quic.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/sni_tree.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/socket.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/udp.o)

file(GLOB_RECURSE USOCKETS_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/src/*.c)

file(GLOB_RECURSE UWEBSOCKETS_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/src/*.h)

add_custom_target(uWebSockets
	DEPENDS ${USOCKETS_OBJECT_FILES})

add_custom_command(OUTPUT ${USOCKETS_OBJECT_FILES}
	COMMAND WITH_OPENSSL=1 WITH_ZLIB=1 make
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets
	DEPENDS ${USOCKETS_SOURCE_FILES} ${UWEBSOCKETS_HEADER_FILES})

add_custom_target(deps
	DEPENDS ShashaDeps)

add_custom_command(OUTPUT ShashaDeps
	COMMAND COMPILE_GNUPLOT=${WITH_COMPILE_GNUPLOT} GNUPLOT_VERSION=${GNUPLOT_VERSION} make
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs)

add_dependencies(Shasha deps uWebSockets)

if (MUSICAT_WITH_CORO) 
	message("-- INFO: Configuring Musicat with coroutines")

	set(DPP_CORO ON)

	target_compile_definitions(Shasha PUBLIC MUSICAT_WITH_CORO)
	set(MUSICAT_CXX_STANDARD 20)
endif()

if (MUSICAT_DEBUG_SYMBOL)
	message("-- INFO: Will build Musicat with debug symbol")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

# Linking libraries
target_link_libraries(Shasha
	dpp
	# oggz
	# ogg
	pq
	curlpp
	curl
	icui18n
	icuuc
	icudata
	icuio
	${USOCKETS_OBJECT_FILES}
	ssl
	crypto
	z
	pthread
	# Add any other libs you want to use here
	)

# Specify includes
target_include_directories(Shasha PRIVATE
	include
	# uWebSockets
	libs
	libs/DPP/include
	libs/yt-search.h/include
	libs/nekos-bestpp/include
	libs/uWebSockets/uSockets/src
	libs/icu/usr/local/include
	libs/curlpp/include
	# libs/liboggz/include
	libs/jwt-cpp/include
	/usr/include/postgresql
	# Remember to add the include directories of any other libraries too
	)

target_link_directories(Shasha PRIVATE
	libs/curlpp/build
	libs/icu/usr/local/lib
	# libs/liboggz/build/lib
	)

# Set C++ version
set_target_properties(Shasha PROPERTIES
	CXX_STANDARD ${MUSICAT_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	EXPORT_COMPILE_COMMANDS ON
	)

# Add DPP as dependency
add_subdirectory(libs/DPP)
# You can also add any other libs you want to use

message(WARNING "If you like this bot or wanna suggest smt, say hi in https://discord.gg/ADGcaHnRFs <3")

# vim: sw=8 noet
