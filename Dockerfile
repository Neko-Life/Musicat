FROM debian:stable-slim

WORKDIR /tmp

RUN apt update && \
    apt install -y curl git libcurl4-openssl-dev zlib1g-dev libssl-dev libpq-dev libogg-dev python3 ffmpeg opus-tools g++ make cmake

RUN curl https://dl.dpp.dev/latest --output ./dpp.deb && \
    curl https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp --output /usr/local/sbin/yt-dlp && \
    curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp --output json.hpp && \
    git clone https://github.com/jpbarrette/curlpp.git && \
    git clone https://github.com/brainboxdotcc/DPP.git

ENV CXX=/usr/bin/g++

RUN chmod +x /usr/local/sbin/yt-dlp && \
    dpkg -i /tmp/dpp.deb && apt -f install && \
    mkdir /tmp/curlpp/build && cd /tmp/curlpp/build && cmake ../ && make && make -j$(nproc) install
    # mkdir /tmp/DPP/build && cd /tmp/DPP/build && cmake ../ -DCMAKE_INSTALL_PREFIX=/usr/local && make -j$(nproc) install

WORKDIR /root/musicat

COPY ./ /root/musicat/

# RUN export CPLUS_INCLUDE_PATH=/usr/include/postgresql:include/nlohmann && \
#     mkdir include/nlohmann && cp /tmp/json.hpp include/nlohmann/ && \
#     make -j$(nproc) all
