#!/bin/bash

export CC=clang
export CXX=clang++
export LDFLAGS='-O0 -g -fuse-ld=mold -stdlib=libc++ -lc++abi'
export CFLAGS='-O0 -g -fuse-ld=mold'
export CXXFLAGS='-O0 -g -stdlib=libc++ -fuse-ld=mold'
