ICU_TGZ = $(PWD)/icu.tgz
ICUDATA_SO = $(PWD)/icu/usr/local/lib/libicudata.so
CURLPP_SO = $(PWD)/curlpp/build/libcurlpp.so

LIBOGGZ_BUILD_DIR = $(PWD)/liboggz/build/
LIBOGGZ_SO = $(LIBOGGZ_BUILD_DIR)lib/liboggz.so
LIBOGGZ_BUILD_ENVS = CC=gcc LDFLAGS=-flto

GNUPLOT_VERSION = 5.4.9
GNUPLOT_DIR = gnuplot-$(GNUPLOT_VERSION)
GNUPLOT_TARFILE = $(GNUPLOT_DIR).tar.gz

curl_dl = curl -L
tar_extract = tar --skip-old-files -xf

all: liboggz icu curlpp gnuplot

icu: $(ICUDATA_SO)

$(ICUDATA_SO): $(ICU_TGZ)
	$(tar_extract) icu.tgz

$(ICU_TGZ):
	$(curl_dl) https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-Ubuntu22.04-x64.tgz -o icu.tgz

curlpp: $(CURLPP_SO)

$(CURLPP_SO):
	cd curlpp && mkdir -p build && cd build && cmake .. && make

liboggz: $(LIBOGGZ_SO)

$(LIBOGGZ_SO):
	cd liboggz && $(LIBOGGZ_BUILD_ENVS) ./autogen.sh && $(LIBOGGZ_BUILD_ENVS) ./configure --prefix=$(LIBOGGZ_BUILD_DIR) && $(LIBOGGZ_BUILD_ENVS) make install

gnuplot: $(GNUPLOT_TARFILE)

$(GNUPLOT_TARFILE):
	$(curl_dl) https://zenlayer.dl.sourceforge.net/project/gnuplot/gnuplot/$(GNUPLOT_VERSION)/gnuplot-$(GNUPLOT_VERSION).tar.gz -o $(GNUPLOT_TARFILE)
	$(tar_extract) $(GNUPLOT_TARFILE)
	# cd curlpp && mkdir -p build && cd build && cmake .. && make
