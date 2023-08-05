all: liboggz icu curlpp

icu: icu/usr/local/lib/libicudata.so

icu/usr/local/lib/libicudata.so: icu.tgz
	tar -xf icu.tgz --skip-old-files

icu.tgz:
	curl -L https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-Ubuntu22.04-x64.tgz -o icu.tgz

curlpp: curlpp/build/libcurlpp.so

curlpp/build/libcurlpp.so:
	cd curlpp && mkdir build && cd build &&	cmake .. && make -j$(nproc)

liboggz: liboggz/build/lib/liboggz.so

liboggz/build/lib/liboggz.so:
	cd liboggz && ./autogen.sh && ./configure --prefix="$(pwd)/build" && make install -j$(nproc)
