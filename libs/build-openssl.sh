#!/bin/sh

echo -- Creating directory: openssl

mkdir openssl
echo -- Entering directory: openssl

cd openssl

echo -- Downloading package: https://github.com/openssl/openssl/releases/download/openssl-3.0.8/openssl-3.0.8.tar.gz
wget 'https://github.com/openssl/openssl/releases/download/openssl-3.0.8/openssl-3.0.8.tar.gz'
tar -xf openssl-3.0.8.tar.gz

echo -- Entering directory: openssl-3.0.8
cd openssl-3.0.8
mkdir -p build

./Configure --prefix=$(pwd)/build --openssldir=$(pwd)/build/ssl \
        --libdir=$(pwd)/build/lib --with-zlib-include=../../zlib/zlib-1.2.13/build/include/ \
        --with-zlib-lib=../../zlib/zlib-1.2.13/build/lib/

make install

echo -- Copying pkgconfig file
cp build/lib/pkgconfig/* ../../pkgconfig

echo -- Leaving directory: openssl-3.0.8
cd ..
echo -- Leaving directory: openssl
cd ..

echo Target openssl built in libs/openssl/openssl-3.0.8/build/lib
echo
