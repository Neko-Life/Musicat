#!/bin/bash

docker run --name musicat -it --rm -v $(pwd)/../../exe:/root/musicat/exe/conf -w /root/musicat/exe/conf \
    -u $(id -nu):$(id -ng) --memory-reservation 256mb -m 256mb --memory-swap 0 \
    shasha/musicat:0.1.0 ../Shasha $@
