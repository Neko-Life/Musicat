#!/bin/sh

echo -- Creating directory: liboggz

mkdir liboggz
echo -- Entering directory: liboggz

cd liboggz

echo -- Downloading package: https://downloads.xiph.org/releases/liboggz/liboggz-1.1.1.tar.gz
wget 'https://downloads.xiph.org/releases/liboggz/liboggz-1.1.1.tar.gz'
tar -xf liboggz-1.1.1.tar.gz

echo -- Entering directory: liboggz-1.1.1
cd liboggz-1.1.1/
mkdir -p build

./configure --enable-shared=no --prefix=$(pwd)/build \
            --with-ogg=libs/libogg/libogg-1.3.5/build \
            --with-ogg-libraries=libs/libogg/libogg-1.3.5/build/lib \
            --with-ogg-includes=libs/libogg/libogg-1.3.5/build/include

make install -j$(nproc)

echo -- Copying pkgconfig file
cp build/lib/pkgconfig/* ../../pkgconfig

echo -- Leaving directory: liboggz-1.1.1
cd ..
echo -- Leaving directory: liboggz
cd ..

echo Target liboggz built in libs/liboggz/liboggz-1.1.1/build/lib
echo
