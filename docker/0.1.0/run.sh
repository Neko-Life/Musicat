#!/bin/bash

check=$(stat ../../exe/music &>/dev/null)
if [ check != 0 ]; then
    mkdir -p ../../exe/music
fi

sudo docker run --name musicat -it --rm -v $(pwd)/../../exe:/root/musicat/exe/conf -w /root/musicat/exe \
    -u $(id -nu):$(id -ng) --memory-reservation 256mb -m 256mb --memory-swap 0 \
    shasha/musicat:0.1.0 /bin/bash -c 'ln -rs /root/musicat/exe/conf/sha_conf.json /root/musicat/exe/sha_conf.json && \
    ln -rs /root/musicat/exe/conf/music /root/musicat/exe/music && ./Shasha' $@
