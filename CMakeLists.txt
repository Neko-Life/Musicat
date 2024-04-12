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
option(MUSICAT_NO_SERVER "Disable Musicat server" OFF)

set(MUSICAT_CXX_STANDARD 17)
set(DPP_INSTALL OFF)
set(DPP_USE_EXTERNAL_JSON ON)

if (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wall /W4")

	# disable server on windows
	set(MUSICAT_NO_SERVER ON)
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

set(WITH_COMPILE_GNUPLOT 0)

if (COMPILE_GNUPLOT)
	set(WITH_COMPILE_GNUPLOT 1)
	set(GNUPLOT_VERSION 6.0.0)

	message("-- INFO: Will compile gnuplot version ${GNUPLOT_VERSION}")
endif()

file(GLOB_RECURSE MUSICAT_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/musicat/*.h)

file(GLOB_RECURSE MUSICAT_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/src/musicat/*.cpp)

# file(GLOB_RECURSE ELIZA_HEADER_FILES
# 	${CMAKE_CURRENT_SOURCE_DIR}/libs/ELIZA/src/*.h)

file(GLOB_RECURSE ELIZA_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/ELIZA/src/eliza.cpp)

list(APPEND MUSICAT_HEADER_FILES 
	"include/nlohmann/json.hpp")

if (MUSICAT_NO_SERVER)
	message("-- INFO: Musicat uWebSockets server disabled")
	add_compile_definitions(MUSICAT_NO_SERVER)
else()
	list(APPEND MUSICAT_HEADER_FILES
		"libs/uWebSockets/uSockets/src/libusockets.h"
		"libs/uWebSockets/uSockets/src/quic.h")
endif()

list(APPEND MUSICAT_SOURCE_FILES
	"libs/yt-search.h/src/yt-search/encode.cpp"
	"libs/yt-search.h/src/yt-search/yt-playlist.cpp"
	"libs/yt-search.h/src/yt-search/yt-search.cpp"
	"libs/yt-search.h/src/yt-search/yt-track-info.cpp"
	"libs/nekos-bestpp/src/nekos-best++.cpp")

# Create an executable
add_executable(Shasha
	# include files
	${MUSICAT_HEADER_FILES}
	# ${ELIZA_HEADER_FILES}
	# !TODO: include/uWebSockets/files

	# impl files
	${MUSICAT_SOURCE_FILES}
	${ELIZA_SOURCE_FILES}
	src/main.cpp)

if (NOT MSVC)
	# uWebSockets
	if (NOT MUSICAT_NO_SERVER)
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

		add_dependencies(Shasha uWebSockets)
	endif()

	# other deps specified in libs/Makefile
	add_custom_command(OUTPUT ShashaDeps
		COMMAND COMPILE_GNUPLOT=${WITH_COMPILE_GNUPLOT} GNUPLOT_VERSION=${GNUPLOT_VERSION} make
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs)

	add_custom_target(deps
		DEPENDS ShashaDeps)
	add_dependencies(Shasha deps)

	add_custom_target(archlinux-dist-folder
		COMMAND mkdir -p archlinux-dist
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

	add_custom_target(archlinux-dist

		COMMAND cp build/Shasha
			build/libs/DPP/library/libdpp.so*
			build/libs/curlpp/libcurlpp.so*
			libs/icu/usr/local/lib/lib*
			src/yt-dlp/ytdlp.py
			archlinux-dist

		COMMAND cp -r libs/yt-dlp
			src/yt-dlp/utils
			archlinux-dist

		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS archlinux-dist-folder Shasha)

	if (COMPILE_GNUPLOT)
		add_custom_target(archlinux-dist-cp-gnuplot

			COMMAND cp libs/gnuplot-${GNUPLOT_VERSION}/build/bin/gnuplot
				archlinux-dist

			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			DEPENDS archlinux-dist-folder Shasha)

		add_dependencies(archlinux-dist archlinux-dist-cp-gnuplot)
	endif()
else()
	# MSVC deps

endif()

if (MUSICAT_WITH_CORO) 
	message("-- INFO: Configuring Musicat with coroutines")

	set(DPP_CORO ON)

	target_compile_definitions(Shasha PUBLIC MUSICAT_WITH_CORO)
	set(MUSICAT_CXX_STANDARD 20)
endif()

if (MUSICAT_DEBUG_SYMBOL)
	message("-- INFO: Will build Musicat with debug symbol")

	# Debug, Release, RelWithDebInfo and MinSizeRel
	set(CMAKE_BUILD_TYPE "RelWithDebInfo")

	if (MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Debug")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
	endif()
else()
	set(CMAKE_BUILD_TYPE "Release")
endif()

if (MSVC)
	# vcpkg cmake confs, PLANNED TO BE UNUSED
	# set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH};${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows)
	# set(FIND_PATH_HINTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")

	# uwebsockets
	# find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h"
	# 	HINTS ${FIND_PATH_HINTS}
	# )
	# target_include_directories(Shasha PRIVATE ${UWEBSOCKETS_INCLUDE_DIRS})

	# curlpp
	# find_package(unofficial-curlpp CONFIG REQUIRED)
	# target_link_libraries(Shasha PRIVATE unofficial::curlpp::curlpp)

	# libpq
	# find_package(PostgreSQL REQUIRED)
	# target_link_libraries(Shasha PRIVATE PostgreSQL::PostgreSQL)

	# manual installation (non-vcpkg)


	# submodules
	target_include_directories(Shasha PRIVATE
		include
		libs
		libs/DPP/include
		libs/yt-search.h/include
		libs/nekos-bestpp/include
		libs/icu/usr/local/include
		libs/ELIZA/src
		# Remember to add the include directories of any other libraries too
		)

else()
	# Linking libraries
	target_link_libraries(Shasha
		dpp
		# oggz
		# ogg
		pq
		curlpp
		curl
		cppcodec
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
		libs/ELIZA/src
		# Remember to add the include directories of any other libraries too
		)

	target_link_directories(Shasha PRIVATE
		libs/icu/usr/local/lib
		)
endif()

# Set C++ version
set_target_properties(Shasha PROPERTIES
	CXX_STANDARD ${MUSICAT_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	EXPORT_COMPILE_COMMANDS ON
	)

# Add submodule cmake dependency
add_subdirectory(libs/DPP)
add_subdirectory(libs/cppcodec)
add_subdirectory(libs/curlpp)

message(WARNING "If you like this bot or wanna suggest smt, say hi in https://discord.gg/ADGcaHnRFs <3")

# add_executable(base32 include/musicat/util/base32.h base32.cpp src/musicat/util/base32.cpp)
# target_link_libraries(base32 cppcodec)
# target_include_directories(base32 PRIVATE
# 	include)

# vim: sw=8 noet
