#!/bin/sh

echo -- Entering directory: curl

cd curl
mkdir -p build

echo -- Entering directory: build
cd build

cmake .. -DBUILD_SHARED_LIBS=OFF \
        -DOPENSSL_INCLUDE_DIR=../../openssl/openssl-3.0.8/include \
        -DOPENSSL_LIBRARIES=../../openssl/openssl-3.0.8/build/lib

make -j$(nproc)

echo -- Leaving directory: build
cd ..
echo -- Leaving directory: curl
cd ..

echo Target curl built in libs/curl/build/lib
echo
