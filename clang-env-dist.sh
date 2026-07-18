#!/bin/bash

export CC=clang
export CXX=clang++
export LDFLAGS='-Oz -flto -fuse-ld=mold -stdlib=libc++ -lc++abi'
export CFLAGS='-Oz -flto -fuse-ld=mold'
export CXXFLAGS='-Oz -flto -stdlib=libc++ -fuse-ld=mold'
