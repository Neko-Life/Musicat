# Musicat - Discord Music Bot written in C++

Discord Music Bot written in C++

- Just install dependencies, compile and fill out `exe/sha_cfg.json`, config file should be in one directory as the binary unless using docker, docker scripts available to use in `docker/`.
- Musicat is fully slash command and need to register the commands to discord, run `./Shasha reg g` or `./Shasha reg <guild_id>` to register slash commands.

## Dependencies

* [DPP](https://github.com/brainboxdotcc/DPP) - Library (clone it into the `libs/` folder)
* [yt-dlp](https://github.com/yt-dlp/yt-dlp) - Should be installed on your machine
* [FFmpeg](https://github.com/FFmpeg/FFmpeg) - Should be installed on your machine
* libopus - Library
* liboggz - Library
* libogg - Library
* libcurl - Library
* [curlpp](https://github.com/jpbarrette/curlpp) - Library
* [libpq](https://github.com/postgres/postgres/tree/master/src/interfaces/libpq) - Library
* libsodium - Library
* openssl - Library
* ICU - Library
* [nlohmann/json](https://github.com/nlohmann/json/tree/develop/single_include/nlohmann) - Headers only, included
* [encode.h](https://gist.github.com/arthurafarias/56fec2cd49a32f374c02d1df2b6c350f) - Included
* [yt-search.h](https://github.com/Neko-Life/yt-search.h) - Included
* opus - Library
* [opusfile](https://github.com/xiph/opusfile) - [optional] Currently unused anywhere as it's bloat.

## Docker

* Build the image
```sh
docker compose build
```
* Configure `exe/sha_conf.json` (or wherever the config file is, you can configure musicat's volumes in `docker-composer.yml`. See `exe/sha_conf.example.json`).
  The `MUSIC_FOLDER` config variable should be `/root/musicat/exe/music/` (again you can configure the volumes in `docker-compose.yml`,
  and other configuration like database username and password should both match between config and postgres)
* Register the commands
```sh
./docker-register.sh g
```
* Run
```sh
docker compose up
```
* One issue using docker that it need some time to connect to the database, maybe I will fix that one day using shorter database connection check interval.

## Compiling
* Clone the DPP repo into `libs/` first if you haven't
```sh
mkdir libs
cd libs
git clone 'https://github.com/brainboxdotcc/DPP' -b dev
cd ..
```
* Clone uWebSockets repo into `libs/`, compile and install it
```sh
cd libs
git clone 'https://github.com/uNetworking/uWebSockets' --recurse-submodules
cd uWebSockets/uSockets
git checkout master
cd ..
WITH_OPENSSL=1 WITH_ZLIB=1 WITH_PROXY=1 WITH_ASAN=1 make -j$(nproc)
sudo make prefix=/usr install
cd ../..
```
* Create a `build/` folder and cd into it
```sh
mkdir build
cd build
```
* Run cmake
```sh
cmake ..
```
* Compile
```sh
make all -j$(nproc)
```
* Move the built binary to `exe/` (or wherever you want along with a config file)
```sh
mv Shasha ../exe
```
* Create and fill the config file (config file must be named `sha_conf.json`, see `exe/sha_conf.example.json`) and run the binary
```sh
# register the commands
./Shasha reg g

# run the bot
./Shasha
```
