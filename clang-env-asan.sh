#!/bin/bash

export CC=clang
export CXX=clang++
export LDFLAGS='-O0 -g -fuse-ld=mold -stdlib=libc++ -lc++abi -fsanitize=address,leak'
export CFLAGS='-O0 -g -fuse-ld=mold -fsanitize=address,leak'
export CXXFLAGS='-O0 -g -stdlib=libc++ -fuse-ld=mold -fsanitize=address,leak'
