#!/bin/sh

mkdir build

./build-libs.sh

cd build
cmake .. -DOPENSSL_INCLUDE_DIR=$(pwd)/../libs/openssl/openssl-3.0.8/build/include \
        -DZLIB_INCLUDE_DIR=$(pwd)/../libs/zlib/zlib-1.2.13/build/include \
        -DZLIB_LIBRARY=$(pwd)/../libs/zlib/zlib-1.2.13/build/lib/libz.a \
        -DOPENSSL_SSL_LIBRARY=$(pwd)/../libs/openssl/openssl-3.0.8/build/lib/libssl.a \
        -DOPENSSL_CRYPTO_LIBRARY=$(pwd)/../libs/openssl/openssl-3.0.8/build/lib/libcrypto.a \
        -DOPUS_INCLUDE_DIRS=$(pwd)/../libs/opus/opus-1.1.2/build/include/ \
        -DOPUS_LIBRARIES=$(pwd)/../libs/opus/opus-1.1.2/build/lib/libopus.a \
        -Dsodium_LIBRARY_RELEASE=$(pwd)/../libs/libsodium/libsodium-stable/build/lib/libsodium.a

make -j$(nproc)

cd ..

echo -- Built executable in directory build/: Shasha
