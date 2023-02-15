#!/bin/sh

echo -- Entering directory: curlpp

cd curlpp
mkdir -p build

echo -- Entering directory: build
cd build

cmake .. -DBUILD_SHARED_LIBS=OFF -DCURL_INCLUDE_DIRS=../../curl/include -DCURL_LIBRARIES=../../curl/build/lib
make curlpp_static -j$(nproc)
echo -- Leaving directory: build
cd ..
echo -- Leaving directory: curlpp
cd ..

echo Target curlpp built in libs/curlpp/build
echo
