# Musicat - Discord Music Bot written in C++

Discord Music Bot written in C++

- Just install dependencies, compile and fill out `exe/sha_cfg.json`, config file should be in one directory as the binary unless using docker, docker scripts available to use in `docker/`.
- Musicat is fully slash command and need to register the commands to discord, run `./Shasha reg g` or `./Shasha reg <guild_id>` to register slash commands.

## Dependencies

* [DPP](https://github.com/brainboxdotcc/DPP) - Library
* [yt-dlp](https://github.com/yt-dlp/yt-dlp) - Should be installed on your machine
* [FFmpeg](https://github.com/FFmpeg/FFmpeg) - Should be installed on your machine
* libopus - Library
* libcurl - Library
* [curlpp](https://github.com/jpbarrette/curlpp) - Library
* [libpq](https://github.com/postgres/postgres/tree/master/src/interfaces/libpq) - Library
* [nlohmann/json](https://github.com/nlohmann/json/tree/develop/single_include/nlohmann) - Headers only
* [encode.h](https://gist.github.com/arthurafarias/56fec2cd49a32f374c02d1df2b6c350f) - Included
* [yt-search.h](https://github.com/Neko-Life/yt-search.h) - Included
* [opusfile](https://github.com/xiph/opusfile) - [optional] Currently unused anywhere as it's bloat.

## Docker

* `cd` to `docker/[version]/` and run `compose.sh` as root to build the image.
* Fill out `exe/sha_conf.json`.
* In the `docker/[version]/ directory, run `./register.sh g` to register Musicat's slash commands to discord globally, or `./register.sh <guild_id>` to register only in one specific guild.
* Run `run.sh` to start the bot.
