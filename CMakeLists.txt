# minimum CMake version required
cmake_minimum_required(VERSION 3.15)
# Project name, version and description
project(Musicat 
	VERSION 0.1.1
	LANGUAGES CXX
	DESCRIPTION "Discord Music Bot written in C++")

option(MUSICAT_WITH_CORO "Configure Musicat with C++20 coroutines" OFF)
option(MUSICAT_DEBUG_SYMBOL "Build Musicat with debug symbol" OFF)

set(MUSICAT_CXX_STANDARD 17)
set(MUSICAT_COMPILE_OPTIONS -Wall -Wextra -Wpedantic)

# Add DPP as dependency
add_subdirectory(libs/DPP)
# You can also add any other libs you want to use

# Create an executable
add_executable(Shasha
	# include files
	include/musicat/config.h
	include/musicat/autocomplete.h
	include/musicat/cmds.h
	include/musicat/db.h
	include/musicat/musicat.h
	include/musicat/pagination.h
	include/musicat/player.h
	include/musicat/runtime_cli.h
	include/musicat/slash.h
	include/musicat/storage.h
	include/musicat/util.h
	include/musicat/server.h
	include/musicat/thread_manager.h
	include/musicat/audio_processing.h
	include/musicat/child.h
	include/musicat/function_macros.h
	include/musicat/search-cache.h
	include/musicat/helper_processor.h
	include/musicat/child/worker.h
	include/musicat/child/command.h
	include/musicat/child/worker_command.h
	include/musicat/child/slave_manager.h
	include/musicat/child/worker_management.h
	include/nlohmann/json.hpp
	libs/uWebSockets/uSockets/src/libusockets.h
	libs/uWebSockets/uSockets/src/quic.h
	# !TODO: include/uWebSockets/files

	# impl files
	src/musicat/autocomplete.cpp
	src/musicat/cli.cpp
	src/musicat/db.cpp
	src/musicat/musicat.cpp
	src/musicat/pagination.cpp
	src/musicat/player.cpp
	src/musicat/player_manager.cpp
	src/musicat/player_manager_embed.cpp
	src/musicat/player_manager_events.cpp
	src/musicat/player_manager_stream.cpp
	src/musicat/player_manager_util.cpp
	src/musicat/run.cpp
	src/musicat/runtime_cli.cpp
	src/musicat/slash.cpp
	src/musicat/storage.cpp
	src/musicat/util-response.cpp
	src/musicat/util.cpp
	src/musicat/server.cpp
	src/musicat/thread_manager.cpp
	src/musicat/audio_processing.cpp
	src/musicat/command.cpp
	src/musicat/child.cpp
	src/musicat/search-cache.cpp
	src/musicat/helper_processor.cpp
	src/musicat/child/worker.cpp
	src/musicat/child/command.cpp
	src/musicat/child/worker_command.cpp
	src/musicat/child/slave_manager.cpp
	src/musicat/child/worker_management.cpp
	src/musicat/cmds/autoplay.cpp
	src/musicat/cmds/bubble_wrap.cpp
	src/musicat/cmds/download.cpp
	src/musicat/cmds/hello.cpp
	src/musicat/cmds/interactive_message.cpp
	src/musicat/cmds/invite.cpp
	src/musicat/cmds/join.cpp
	src/musicat/cmds/loop.cpp
	src/musicat/cmds/move.cpp
	src/musicat/cmds/pause.cpp
	src/musicat/cmds/play.cpp
	src/musicat/cmds/playlist.cpp
	src/musicat/cmds/queue.cpp
	src/musicat/cmds/remove.cpp
	src/musicat/cmds/search.cpp
	src/musicat/cmds/skip.cpp
	src/musicat/cmds/stop.cpp
	src/musicat/cmds/image.cpp
	src/musicat/cmds/seek.cpp
	src/musicat/cmds/progress.cpp
	src/musicat/cmds/volume.cpp
	src/musicat/cmds/filters.cpp
	src/musicat/cmds/filters/equalizer.cpp
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
	COMMAND WITH_OPENSSL=1 WITH_ZLIB=1 WITH_PROXY=1 WITH_ASAN=1 make
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets
	DEPENDS ${USOCKETS_SOURCE_FILES} ${UWEBSOCKETS_HEADER_FILES})

add_custom_target(deps
	DEPENDS ShashaDeps)

add_custom_command(OUTPUT ShashaDeps
	COMMAND make
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs)

add_dependencies(Shasha deps uWebSockets)

if (MUSICAT_WITH_CORO) 
	message("-- INFO: Configuring Musicat with coroutines")

	target_compile_definitions(dpp PUBLIC DPP_CORO=ON)
	set(DPP_COMPILE_OPTIONS -std=c++20 ${DPP_COMPILE_OPTIONS})

	target_compile_definitions(Shasha PRIVATE DPP_CORO=ON)
	set(MUSICAT_CXX_STANDARD 20)
endif()

if (MUSICAT_DEBUG_SYMBOL)
	message("-- INFO: Will build Musicat with debug symbol")
	set(MUSICAT_COMPILE_OPTIONS -g ${MUSICAT_COMPILE_OPTIONS})
	set(DPP_COMPILE_OPTIONS -g ${DPP_COMPILE_OPTIONS})
endif()

if (DPP_COMPILE_OPTIONS)
	target_compile_options(dpp PUBLIC ${DPP_COMPILE_OPTIONS})
endif()

# Linking libraries
target_link_libraries(Shasha
	dpp
	oggz
	ogg
	pq
	curlpp
	curl
	icui18n
	icuuc
	icudata
	icuio
	${USOCKETS_OBJECT_FILES}
	z
	ssl
	crypto
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
	libs/liboggz/include
	/usr/include/postgresql
	# Remember to add the include directories of any other libraries too
	)

target_link_directories(Shasha PRIVATE
	libs/curlpp/build
	libs/icu/usr/local/lib
	libs/liboggz/build/lib
	)

target_compile_options(Shasha PRIVATE ${MUSICAT_COMPILE_OPTIONS})

# Set C++ version
set_target_properties(Shasha PROPERTIES
	CXX_STANDARD ${MUSICAT_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	EXPORT_COMPILE_COMMANDS ON
	)
