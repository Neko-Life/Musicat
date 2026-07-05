#!/bin/bash

export CC=clang
export CXX=clang++
export LDFLAGS='-flto -fuse-ld=mold -stdlib=libc++ -lc++abi -fsanitize=address,leak'
export CFLAGS='-flto -fuse-ld=mold -fsanitize=address,leak'
export CXXFLAGS='-flto -stdlib=libc++ -fuse-ld=mold -fsanitize=address,leak'
