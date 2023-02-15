#!/bin/sh

echo -- Creating directory: libogg

mkdir libogg
echo -- Entering directory: libogg

cd libogg

echo -- Downloading package: https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz
wget 'https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz'
tar -xf libogg-1.3.5.tar.gz

echo -- Entering directory: libogg-1.3.5
cd libogg-1.3.5
mkdir -p build

./configure --enable-shared=no --prefix=$(pwd)/build
make install -j$(nproc)

echo -- Copying pkgconfig file
cp build/lib/pkgconfig/* ../../pkgconfig

echo -- Leaving directory: libogg-1.3.5
cd ..
echo -- Leaving directory: libogg
cd ..

echo Target libogg built in libs/libogg/libogg-1.3.5/build/lib
echo
