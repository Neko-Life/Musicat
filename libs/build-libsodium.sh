#!/bin/sh

echo -- Creating directory: libsodium

mkdir libsodium
echo -- Entering directory: libsodium

cd libsodium

echo -- Downloading package: https://download.libsodium.org/libsodium/releases/libsodium-1.0.18-stable.tar.gz
wget 'https://download.libsodium.org/libsodium/releases/libsodium-1.0.18-stable.tar.gz'
tar -xf libsodium-1.0.18-stable.tar.gz

echo -- Entering directory: libsodium-stable
cd libsodium-stable/
mkdir -p build

./configure --enable-shared=no --prefix $(pwd)/build
make install -j$(nproc)

echo -- Copying pkgconfig file
cp build/lib/pkgconfig/* ../../pkgconfig

echo -- Leaving directory: libsodium-stable
cd ..
echo -- Leaving directory: libsodium
cd ..

echo Target libsodium built in libs/libsodium/libsodium-stable/build/lib
echo
