!#/bin/sh

chmod +x libs/build-*.sh
echo -- Entering directory: libs
cd libs

mkdir -p pkgconfig

cat build-libsodium.sh
./build-libsodium.sh

cat build-opus.sh
./build-opus.sh

cat build-zlib.sh
./build-zlib.sh

cat build-openssl.sh
./build-openssl.sh

cat build-curl.sh
./build-curl.sh

cat build-curlpp.sh
./build-curlpp.sh

cat build-libogg.sh
./build-libogg.sh

echo -- Leaving directory: libs
cd ..

echo Done
