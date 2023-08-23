# minimum CMake version required
cmake_minimum_required(VERSION 3.15)
# Project name, version and description
project(Musicat 
        VERSION 0.1.1
        LANGUAGES CXX
        DESCRIPTION "Discord Music Bot written in C++")
 
# Add DPP as dependency
add_subdirectory(libs/DPP)
# You can also add any other libs you want to use
 
# Create an executable
add_executable(Shasha
        # include files
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
        libs/yt-search.h/src/yt-search/encode.cpp
        libs/yt-search.h/src/yt-search/yt-playlist.cpp
        libs/yt-search.h/src/yt-search/yt-search.cpp
        libs/yt-search.h/src/yt-search/yt-track-info.cpp
        libs/nekos-bestpp/src/nekos-best++.cpp
        src/main.cpp)

add_executable(exsox
        src/sox-experiment/src/main.cpp)

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

add_dependencies(exsox deps)
 
# Linking libraries
target_link_libraries(Shasha
    dpp
    sox
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

target_link_libraries(exsox
    sox
)
 
# Specify includes
target_include_directories(Shasha PRIVATE
  include
  # uWebSockets
  libs/
  libs/DPP/include
  libs/yt-search.h/include
  libs/nekos-bestpp/include
  libs/uWebSockets/uSockets/src
  libs/icu/usr/local/include
  libs/curlpp/include/
  libs/liboggz/include
  libs/sox/include
  /usr/include/postgresql
  # Remember to add the include directories of any other libraries too
)

target_include_directories(exsox PRIVATE
  libs/sox/include
)

target_link_directories(Shasha PRIVATE
  libs/curlpp/build
  libs/icu/usr/local/lib
  libs/liboggz/build/lib
  libs/sox/build/lib
)

target_link_directories(exsox PRIVATE
  libs/sox/build/lib
)

target_compile_options(Shasha PRIVATE -g -Wall -Wextra -Wpedantic)

target_compile_options(exsox PRIVATE -g -Wall -Wextra -Wpedantic)
 
# Set C++ version
set_target_properties(Shasha PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    EXPORT_COMPILE_COMMANDS ON
)

set_target_properties(exsox PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    EXPORT_COMPILE_COMMANDS ON
)
