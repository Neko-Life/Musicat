FROM archlinux:base-devel as build

# Build dependencies
WORKDIR /app

# Copy source files
COPY include ./include
COPY src ./src
COPY libs ./libs
COPY CMakeLists.txt ./

# Install dependencies
RUN pacman -Syu --noconfirm reflector && reflector --save /etc/pacman.d/mirrorlist && \
      pacman -S --needed --noconfirm libc++ git cmake libsodium opus postgresql-libs clang && \
      mkdir -p build && cd build && \
      export CC=clang && \
      export CXX=clang++ && \
      export LDFLAGS='-flto -stdlib=libc++ -lc++' && \
      export CFLAGS='-flto' && \
      export CXXFLAGS='-flto -stdlib=libc++' && \
      cmake .. && make all -j12

FROM archlinux:base as deploy

RUN pacman -Syu --noconfirm reflector && reflector --save /etc/pacman.d/mirrorlist && \
      pacman -S --noconfirm libc++ postgresql-libs libsodium opus ffmpeg && \
      groupadd musicat && useradd -m -g musicat musicat

USER musicat

WORKDIR /home/musicat

COPY --chown=musicat:musicat --from=build \
             /app/build/Shasha \
             /app/build/libs/DPP/library/libdpp.so* \
             /app/libs/curlpp/build/libcurlpp.so* \
             /home/musicat
#             /app/libs/liboggz/build/lib/liboggz.so* \

COPY --chown=musicat:musicat --from=build \
             /app/libs/yt-dlp \
             /home/musicat/yt-dlp/

ENV LD_LIBRARY_PATH=/home/musicat

VOLUME ["/home/musicat/music"]

# config for container create `-v` switch: /home/musicat/sha_conf.json

CMD ./Shasha
