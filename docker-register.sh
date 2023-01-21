#!/bin/bash

docker run --name musicat -it --rm -v $(pwd)/exe/sha_conf.json:/root/musicat/exe/sha_conf.json \
    -u $(id -nu):$(id -ng) --memory-reservation 128M -m 256M --memory-swap 512M \
    shasha/musicat:0.1.1 ./Shasha reg $@
