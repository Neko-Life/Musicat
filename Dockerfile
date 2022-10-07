FROM archlinux:base-devel

RUN pacman -Syu --noconfirm reflector && reflector --save /etc/pacman.d/mirrorlist

WORKDIR /tmp

#   Download dependencies
RUN pacman -S --noconfirm git cmake python3 python-pip ffmpeg opus postgresql-libs libsodium zlib openssl && \
    python3 -m pip install -U yt-dlp && \
    curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp --output json.hpp && \
    git clone https://github.com/brainboxdotcc/DPP.git && \
    git clone https://github.com/jpbarrette/curlpp.git

# Build dependencies
#   build DPP
RUN mkdir /tmp/DPP/build && cd /tmp/DPP/build && \
    cmake ../ && \
    make -j$(nproc) install && \
#   build curlpp
    mkdir /tmp/curlpp/build && cd /tmp/curlpp/build && \
    cmake ../ && \
    make && make -j$(nproc) install

WORKDIR /root/musicat

# Copy source files
COPY include ./include
COPY src ./src
COPY Makefile compile_flags.txt ./

# Build Musicat
#   copy downloaded json.hpp to local `include/` folder
RUN mkdir -p include/nlohmann && cp /tmp/json.hpp include/nlohmann/ && \
#   build
    mkdir exe && \
    make -j$(nproc) all

WORKDIR /root/musicat/exe
ENV LD_LIBRARY_PATH=/usr/local/lib

CMD ./Shasha
