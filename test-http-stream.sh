#!/bin/bash

SERVER_ID=$1

for ((i=0; i<10000; i++)) ; do
  curl 'http://localhost:9001/stream/'$SERVER_ID -o - > /dev/null &
  echo "Instance $i spawned"
done

sleep 1
curl 'http://localhost:9001/stream/'$SERVER_ID -vv -o - | ffmpeg -i - -f s16le - | aplay -f dat
killall curl
