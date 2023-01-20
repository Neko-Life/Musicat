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
        src/main.cpp)

 
# Linking libraries
target_link_libraries(Shasha
    dpp
    curl
    curlpp
    ogg
    pq
    icui18n
    icuuc
    icudata
    icuio
    # Add any other libs you want to use here
)
 
# Specify includes
target_include_directories(Shasha PRIVATE
    libs/DPP/include
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
