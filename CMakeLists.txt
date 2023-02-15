# minimum CMake version required
cmake_minimum_required(VERSION 3.15)
# Project name, version and description
project(Musicat 
        VERSION 0.1.1
        LANGUAGES CXX
        DESCRIPTION "Discord Music Bot written in C++")

set(BUILD_STATIC_LIBS ON)
set(BUILD_SHARED_LIBS OFF)

set(ENV{PKG_CONFIG_PATH} "${CMAKE_SOURCE_DIR}/libs/pkgconfig")

# set(PKG_CONFIG_PATH libs/opus/opus-1.1.2/build/lib
# libs/libsodium/libsodium-stable/build/lib
# libs/zlib/zlib-1.2.13/build/lib
# libs/openssl/openssl-3.0.8/build/lib
# libs/opus/opus-1.1.2/build/include
# libs/openssl/openssl-3.0.8/include
# libs/zlib/zlib-1.2.13/build/include
# libs/libsodium/libsodium-stable/build/include)

# Create an executable
add_executable(Shasha
# include files
        include/musicat/autocomplete.h
        include/musicat/cmds.h
        include/musicat/db.h
        include/musicat/encode.h
        include/musicat/musicat.h
        include/musicat/pagination.h
        include/musicat/player.h
        include/musicat/runtime_cli.h
        include/musicat/slash.h
        include/musicat/storage.h
        include/musicat/util.h
        include/musicat/yt-playlist.h
        include/musicat/yt-search.h
        include/musicat/yt-track-info.h
        include/nlohmann/json.hpp
        include/nekos-best++.hpp

# impl files
        src/musicat/autocomplete.cpp
        src/musicat/cli.cpp
        src/musicat/db.cpp
        src/musicat/encode.cpp
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
        src/musicat/util.cpp
        src/musicat/yt-playlist.cpp
        src/musicat/yt-search.cpp
        src/musicat/yt-track-info.cpp
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
        src/nekos-bestpp/nekos-best++.cpp
        src/main.cpp)

# target_link_libraries(dpp PRIVATE
#                       libs/zlib/zlib-1.2.13/build/lib/libz.a
#                       libs/opus/opus-1.1.2/build/lib/libopus.a
#                       libs/openssl/openssl-3.0.8/build/lib/libcrypto.a
#                       libs/openssl/openssl-3.0.8/build/lib/libssl.a
#                       libs/libsodium/libsodium-stable/build/lib/libsodium.a
#                       )

# target_include_directories(dpp PRIVATE
#                            libs/DPP/include
#                            libs/opus/opus-1.1.2/build/include
#                            libs/libsodium/libsodium-stable/build/include
#                            libs/zlib/zlib-1.2.13/build/include
#                            libs/openssl/openssl-3.0.8/include
# # Remember to add the include directories of any other libraries too
#                            )

# Linking libraries
target_link_libraries(Shasha PRIVATE
                      # libstdc++-static
                      # -static
                      # -static-libgcc
                      -Wl,-Bstatic
                      dpp
                      /usr/lib/libpq.a
                      /usr/lib/libpgcommon.a
                      /usr/lib/libpgport.a
                      "${CMAKE_SOURCE_DIR}/libs/liboggz/liboggz-1.1.1/build/lib/liboggz.a"
                      "${CMAKE_SOURCE_DIR}/libs/libogg/libogg-1.3.5/build/lib/libogg.a"
                      "${CMAKE_SOURCE_DIR}/libs/curlpp/build/libcurlpp.a"
                      -Wl,-Bdynamic
                      icuio
                      icui18n
                      icuuc
                      icudata
                      -L/usr/local/lib -lcurl -lc -lgcc -lgcc_s -lidn2 -llber -lldap /usr/lib/libssl.so /usr/lib/libcrypto.so /usr/lib/libz.so /usr/lib/libpsl.so /usr/lib/libssh2.so -lgcc -lgcc_s -lc -lgcc -lgcc_s -lidn2 -llber -lldap /usr/lib/libssl.so /usr/lib/libcrypto.so /usr/lib/libz.so /usr/lib/libpsl.so /usr/lib/libssh2.so
                      # Add any other libs you want to use here
                      )
# target_link_libraries(Shasha PRIVATE
#                       # libstdc++-static
#                       -static
#                       -static-libgcc
#                       -Wl,-Bstatic
#                       dpp
#                       /usr/lib/libpq.a
#                       /usr/lib/libpgcommon.a
#                       /usr/lib/libpgport.a
#                       "${CMAKE_SOURCE_DIR}/libs/liboggz/liboggz-1.1.1/build/lib/liboggz.a"
#                       "${CMAKE_SOURCE_DIR}/libs/libogg/libogg-1.3.5/build/lib/libogg.a"
#                       "${CMAKE_SOURCE_DIR}/libs/curlpp/build/libcurlpp.a"
#                       "${CMAKE_SOURCE_DIR}/libs/curl/build/lib/libcurl.a"
#                       "${OPENSSL_SSL_LIBRARY}"
#                       "${OPENSSL_CRYPTO_LIBRARY}"
#                       pthread
#                       dl
#                       -Wl,-Bdynamic
#                       /usr/lib/libm.a
#                       # libs//lib/libicui18n.a
#                       # libs//lib/libicuuc.a
#                       # libs//lib/libicudata.a
#                       # libs//lib/libicuio.a
#                       # Add any other libs you want to use here
#                       )

# Specify includes
target_include_directories(Shasha PRIVATE
                           libs/DPP/include
                           libs/curl/include
                           libs/curlpp/include
                           libs/libogg/libogg-1.3.5/build/include
                           libs/liboggz/liboggz-1.1.1/build/include
                           include
# Remember to add the include directories of any other libraries too
                           )

target_compile_options(Shasha PRIVATE -g -Wall -Wextra -Wpedantic)

# Set C++ version
set_target_properties(Shasha PROPERTIES
                      CXX_STANDARD 17
                      CXX_STANDARD_REQUIRED ON
                      EXPORT_COMPILE_COMMANDS ON
                      )

# Add DPP as dependency
add_subdirectory(libs/DPP)
# You can also add any other libs you want to use

# vim: sw=8 ts=8 et
