#!/bin/bash

export CC=clang
export CXX=clang++
export LDFLAGS='-flto -fuse-ld=mold -stdlib=libc++ -lc++'
export CFLAGS='-flto -fuse-ld=mold'
export CXXFLAGS='-flto -stdlib=libc++ -fuse-ld=mold'
