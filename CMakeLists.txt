# minimum CMake version required
cmake_minimum_required(VERSION 3.15)
# Project name, version and description
project(Musicat 
	VERSION 0.1.1
	LANGUAGES CXX
	DESCRIPTION "Discord Music Bot written in C++")

option(MUSICAT_WITH_CORO "Configure Musicat with C++20 coroutines" ON)
option(COMPILE_GNUPLOT "Download and compile gnuplot" OFF)
option(COMPILE_VERBOSE "Show compile warnings verbosely" OFF)

# config.h defines
option(WITH_WS_P_ETF "Use ETF Websocket Protocol to communicate with Discord" OFF)
option(WITH_AUDIO_INPUT_USE_EXCITER "Use exciter effect to ffmpeg decoder" ON)
option(WITH_UWS_HTTPRESPONSE_NO_WRITEMARK "Make uWebSockets server response anonymous" ON)


set(MUSICAT_CXX_STANDARD 17)
set(DPP_INSTALL OFF)
set(DPP_USE_EXTERNAL_JSON ON)

set(WITH_COMPILE_GNUPLOT 0)

if (COMPILE_GNUPLOT)
	set(WITH_COMPILE_GNUPLOT 1)
	set(GNUPLOT_VERSION 6.0.3)

	message("-- INFO: Will compile gnuplot version ${GNUPLOT_VERSION}")
endif()

# remove dpp json include folder to make sure we don't link against it
set(RM_NLOHMANN_JSON_INCLUDE_CMD "rm" "-rf" "${CMAKE_CURRENT_SOURCE_DIR}/libs/DPP/include/dpp/nlohmann")
list(JOIN  RM_NLOHMANN_JSON_INCLUDE_CMD " " RM_NLOHMANN_JSON_INCLUDE_CMD_STR)
message("-- Executing '${RM_NLOHMANN_JSON_INCLUDE_CMD_STR}'")
execute_process(COMMAND ${RM_NLOHMANN_JSON_INCLUDE_CMD})


# globally include nlohmann
include_directories(include)


file(GLOB_RECURSE MUSICAT_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/musicat/*.h)

file(GLOB_RECURSE MUSICAT_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/src/musicat/*.cpp)

# file(GLOB_RECURSE ELIZA_HEADER_FILES
# 	${CMAKE_CURRENT_SOURCE_DIR}/libs/ELIZA/src/*.h)

file(GLOB_RECURSE ELIZA_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/ELIZA/src/eliza.cpp)

# Create an executable
add_executable(Shasha
	# include files
	${MUSICAT_HEADER_FILES}
	include/nlohmann/json.hpp
	libs/uWebSockets/uSockets/src/libusockets.h
	libs/uWebSockets/uSockets/src/quic.h
	# ${ELIZA_HEADER_FILES}
	# !TODO: include/uWebSockets/files

	# impl files
	${MUSICAT_SOURCE_FILES}
	libs/yt-search.h/src/yt-search/encode.cpp
	libs/yt-search.h/src/yt-search/yt-playlist.cpp
	libs/yt-search.h/src/yt-search/yt-search.cpp
	libs/yt-search.h/src/yt-search/yt-track-info.cpp
	libs/nekos-bestpp/src/nekos-best++.cpp
	${ELIZA_SOURCE_FILES}
	src/main.cpp)

set(USOCKETS_OBJECT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/bsd.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/context.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/epoll_kqueue.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/gcd.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/io_context.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/io_loop.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/io_socket.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/libuv.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/loop.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/openssl.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/quic.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/socket.o
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/udp.o)

file(GLOB_RECURSE USOCKETS_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/src/*.c)

file(GLOB_RECURSE UWEBSOCKETS_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/src/*.h)

add_custom_target(uWebSockets
	DEPENDS ${USOCKETS_OBJECT_FILES})

set(WITH_LTO 0)

if($ENV{LDFLAGS} MATCHES "-flto")
	set(WITH_LTO 1)
endif()

add_custom_command(OUTPUT ${USOCKETS_OBJECT_FILES}
	COMMAND WITH_LTO=${WITH_LTO} make
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets
	DEPENDS ${USOCKETS_SOURCE_FILES} ${UWEBSOCKETS_HEADER_FILES})

add_custom_target(libs
	COMMAND WITH_LTO=${WITH_LTO} COMPILE_GNUPLOT=${WITH_COMPILE_GNUPLOT} GNUPLOT_VERSION=${GNUPLOT_VERSION} make
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs)

add_dependencies(Shasha libs uWebSockets)

add_custom_target(archlinux-dist-folder
	COMMAND mkdir -p archlinux-dist
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(archlinux-dist

	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/Shasha
		${CMAKE_CURRENT_BINARY_DIR}/libs/DPP/library/libdpp.so*
		libs/curlpp/build/libcurlpp.so*
		libs/icu/usr/local/lib/lib*
		libs/libopusenc/build/lib/libopusenc.so*
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

if (MUSICAT_WITH_CORO) 
	message("-- INFO: Configuring Musicat with coroutines")

	set(DPP_CORO ON)

	target_compile_definitions(Shasha PUBLIC MUSICAT_WITH_CORO)
	target_compile_definitions(Shasha PUBLIC DPP_CORO)

	set(MUSICAT_CXX_STANDARD 20)
endif()

if (COMPILE_VERBOSE)
	message("-- INFO: Compiling Musicat with verbose warnings")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
endif()

if (WITH_WS_P_ETF)
	message("-- INFO: Configuring Musicat WITH_WS_P_ETF")
	target_compile_definitions(Shasha PUBLIC MUSICAT_WS_P_ETF)
endif()

if(WITH_AUDIO_INPUT_USE_EXCITER)
	message("-- INFO: Configuring Musicat WITH_AUDIO_INPUT_USE_EXCITER")
	target_compile_definitions(Shasha PUBLIC AUDIO_INPUT_USE_EXCITER)
endif()

if(WITH_UWS_HTTPRESPONSE_NO_WRITEMARK)
	message("-- INFO: Configuring Musicat WITH_UWS_HTTPRESPONSE_NO_WRITEMARK")
	target_compile_definitions(Shasha PUBLIC UWS_HTTPRESPONSE_NO_WRITEMARK)
endif()


# check for python
if (NOT HAVE_PYTHON)
	set(Python_USE_STATIC_LIBS FALSE)
	find_package (Python COMPONENTS Interpreter Development)
endif()

if (NOT HAVE_PYTHON AND Python_FOUND AND Python_Development.Module_FOUND AND Python_Development.Module_FOUND)
	set(HAVE_PYTHON 1)

	set(PYTHON_VERSION ${Python_VERSION})
	set(PYTHON_VERSION_MAJOR ${Python_VERSION_MAJOR})
	set(PYTHON_VERSION_MINOR ${Python_VERSION_MINOR})
	set(PYTHON_VERSION_PATCH ${Python_VERSION_PATCH})

	if (NOT PYTHON_CMD)
		set(PYTHON_CMD ${Python_EXECUTABLE})
	endif()
	if (NOT PYTHON_INCLUDE)
		set(PYTHON_INCLUDE ${Python_INCLUDE_DIRS})
	endif()
	if (NOT PYTHON_LIB)
		set(PYTHON_LIB ${Python_LIBRARIES})
	endif()
endif()

if (HAVE_PYTHON)
	if (NOT PYTHON_CMD)
		message(FATAL_ERROR "Required to define PYTHON_CMD")
	endif()

	if (NOT PYTHON_INCLUDE)
		message(FATAL_ERROR "Required to define PYTHON_INCLUDE")
	endif()

	if (NOT PYTHON_LIB)
		message(FATAL_ERROR "Required to define PYTHON_LIB")
	endif()

	if (NOT PYTHON_VERSION)
		set(PYTHON_VERSION "${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.${PYTHON_VERSION_PATCH}")
		string(REGEX MATCH "^[0-9]+\.[0-9]+\.[0-9]+$" PYTHON_VERS "${PYTHON_VERSION}")
		if (NOT PYTHON_VERS)
			message(WARNING "Invalid PYTHON_VERSION: ${PYTHON_VERSION}")
			message(FATAL_ERROR "Required to define PYTHON_VERSION_MAJOR, PYTHON_VERSION_MINOR and PYTHON_VERSION_PATCH")
		endif()
	endif()

	target_compile_definitions(Shasha PUBLIC MUSICAT_WITH_PYTHON)

	message("-- INFO: PYTHON_CMD: ${PYTHON_CMD}")
	message("-- INFO: PYTHON_VERSION: ${PYTHON_VERSION}")
	message("-- INFO: PYTHON_INCLUDE: ${PYTHON_INCLUDE}")
	message("-- INFO: PYTHON_LIB: ${PYTHON_LIB}")
else()
	message(WARNING "No python installation detected, ytdlp invocation will fallback using child process")
endif()



# Linking libraries
target_link_libraries(Shasha
	dpp
	opusenc
	# oggz
	# ogg
	avformat
	avfilter
	avcodec
	avutil
	swresample
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
	libs/libopusenc/include
	/usr/include/opus
	/usr/include/postgresql
	${PYTHON_INCLUDE}
	libs/ELIZA/src
	"${CMAKE_CURRENT_BINARY_DIR}/include"
	# Remember to add the include directories of any other libraries too
	)

target_link_directories(Shasha PRIVATE
	libs/curlpp/build
	libs/icu/usr/local/lib
	# libs/liboggz/build/lib
	libs/libopusenc/build/lib
	)

# Set C++ version
set_target_properties(Shasha PROPERTIES
	CXX_STANDARD ${MUSICAT_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	EXPORT_COMPILE_COMMANDS ON
	)



# Add DPP as dependency
add_subdirectory(libs/DPP)
add_subdirectory(libs/cppcodec)

# libogg build options
# set(INSTALL_DOCS OFF)
# set(INSTALL_PKG_CONFIG_MODULE OFF)
# set(INSTALL_CMAKE_PACKAGE_MODULE OFF)
# add_subdirectory(libs/ogg)

add_subdirectory(src/stream-codec)
if (HAVE_PYTHON)
	add_subdirectory(src/yt-dlp)
endif()

# You can also add any other libs you want to use

message(WARNING "If you like this bot or wanna suggest smt, say hi in https://discord.gg/ADGcaHnRFs <3")

configure_file(include/musicat/config.h.in include/musicat/config.h)

# add_executable(base32 include/musicat/util/base32.h base32.cpp src/musicat/util/base32.cpp)
# target_link_libraries(base32 cppcodec)
# target_include_directories(base32 PRIVATE
# 	include)

# vim: sw=8 noet
