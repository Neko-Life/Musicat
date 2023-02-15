#!/bin/sh

echo -- Creating directory: opus

mkdir opus
echo -- Entering directory: opus

cd opus

echo -- Downloading package: https://github.com/xiph/opus/releases/download/v1.1.2/opus-1.1.2.tar.gz
wget 'https://github.com/xiph/opus/releases/download/v1.1.2/opus-1.1.2.tar.gz'
tar -xf opus-1.1.2.tar.gz

echo -- Entering directory: opus-1.1.2
cd opus-1.1.2
mkdir -p build

./configure --enable-shared=no --prefix $(pwd)/build
make install -j$(nproc)

echo -- Copying pkgconfig file
cp build/lib/pkgconfig/* ../../pkgconfig

echo -- Leaving directory: opus-1.1.2
cd ..
echo -- Leaving directory: opus
cd ..

echo Target opus built in libs/opus/opus-1.1.2/build/lib
echo
