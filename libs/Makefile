all: icu curlpp

icu: icu/usr/local/lib/libicudata.so.73.2

icu/usr/local/lib/libicudata.so.73.2: icu.tgz
	tar -xf icu.tgz --skip-old-files

icu.tgz:
	curl -L https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-Ubuntu22.04-x64.tgz -o icu.tgz

curlpp: curlpp/build/libcurlpp.so

curlpp/build/libcurlpp.so:
	cd curlpp && mkdir build && cd build &&	cmake .. && make -j$(nproc)
