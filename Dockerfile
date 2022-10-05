FROM archlinux:base-devel

RUN pacman -Syu --noconfirm reflector && reflector --save /etc/pacman.d/mirrorlist

WORKDIR /tmp

RUN pacman -S --noconfirm git cmake python3 python-pip ffmpeg opus

RUN git clone https://github.com/brainboxdotcc/DPP.git && \
    mkdir /tmp/DPP/build && cd /tmp/DPP/build && \
    cmake ../ && \
    make -j$(nproc) install

RUN python3 -m pip install -U yt-dlp

RUN git clone https://github.com/jpbarrette/curlpp.git && \
    mkdir /tmp/curlpp/build && \
    cd /tmp/curlpp/build && \
    cmake ../ && \
    make && make -j$(nproc) install

RUN pacman -S --noconfirm postgresql-libs

RUN curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp --output json.hpp

WORKDIR /root/musicat

COPY include ./include
COPY src ./src
COPY Makefile compile_flags.txt ./

RUN mkdir -p include/nlohmann && cp /tmp/json.hpp include/nlohmann/ && \
    mkdir exe && \
    make -j$(nproc) all

WORKDIR /root/musicat/exe
ENV LD_LIBRARY_PATH=/usr/local/lib

CMD echo "{\"SHA_TKN\":\"$BOT_TOKEN\",\"SHA_ID\":$BOT_CLIENT_ID,\"SHA_DB\":\"$DB_CONNECTION_STRING\"}" > sha_conf.json && ./Shasha
