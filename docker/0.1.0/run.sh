#!/bin/bash

cd ../../
if [ ! -d ./exe/music ]; then
    mkdir -p ./exe/music
fi

docker compose up $@
