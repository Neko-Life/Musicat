#!/bin/sh

echo -- Creating directory: zlib

mkdir zlib
echo -- Entering directory: zlib

cd zlib

echo -- Downloading package: https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz 
wget 'https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz'
tar -xf zlib-1.2.13.tar.gz

echo -- Entering directory: zlib-1.2.13
cd zlib-1.2.13
mkdir -p build

./configure --static --64 --prefix $(pwd)/build
make install -j$(nproc)

echo -- Leaving directory: zlib-1.2.13
cd ..
echo -- Leaving directory: zlib
cd ..

echo Target libsodium built in libs/zlib/zlib-1.2.13/build/lib
echo
