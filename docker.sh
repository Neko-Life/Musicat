#!/bin/bash

# example setting up with db without docker compose

# build the image and pull a postgres image
docker build . -t shasha/musicat:latest
docker image pull postgres:14.5-bullseye

# create containers
docker container create --net net1 -p 9001:9001 -v ./exe/sha_conf.docker.json:/root/Musicat/exe/sha_conf.json -v ./exe/music:/root/music --name Musicat shasha/musicat:latest
docker container create --net net1 -p 5432:5432 -v /var/lib/postgres/data:/var/lib/postgres/data -e POSTGRES_USER=musicat -e POSTGRES_PASSWORD=musicat -e POSTGRES_DB=musicat --name db postgres:14.5-bullseye

# create network
docker network create net1

# start
docker container start Musicat db
