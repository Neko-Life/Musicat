ICU_TGZ = $(PWD)/icu.tgz
ICUDATA_SO = $(PWD)/icu/usr/local/lib/libicudata.so

CURLPP_SO = $(PWD)/curlpp/build/libcurlpp.so

LIBOGGZ_BUILD_DIR = $(PWD)/liboggz/build/
LIBOGGZ_SO = $(LIBOGGZ_BUILD_DIR)lib/liboggz.so
LIBOGGZ_BUILD_ENVS = CC=gcc LDFLAGS=-flto

GNUPLOT_DIR = gnuplot-$(GNUPLOT_VERSION)
GNUPLOT_BUILD_DIR = $(PWD)/$(GNUPLOT_DIR)/build/
GNUPLOT_BIN = $(GNUPLOT_BUILD_DIR)bin/gnuplot
GNUPLOT_CONFIGURE = $(PWD)/$(GNUPLOT_DIR)/configure
GNUPLOT_TARFILE = $(GNUPLOT_DIR).tar.gz
GNUPLOT_BUILD_ENVS = CC=gcc LDFLAGS= CFLAGS=

curl_dl = curl -L
tar_extract = tar --skip-old-files -xf

ifeq ($(COMPILE_GNUPLOT),1)
	override GNUPLOT_TARGET = gnuplot
endif

all: icu $(GNUPLOT_TARGET) # curlpp liboggz 

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

gnuplot: $(GNUPLOT_BIN)

$(GNUPLOT_BIN): $(GNUPLOT_CONFIGURE)
	cd $(GNUPLOT_DIR) && $(GNUPLOT_BUILD_ENVS) ./configure --prefix=$(GNUPLOT_BUILD_DIR) --with-texdir=$(GNUPLOT_BUILD_DIR)texmf --with-qt=no --with-x=no && $(GNUPLOT_BUILD_ENVS) make install

$(GNUPLOT_CONFIGURE): $(GNUPLOT_TARFILE)
	$(tar_extract) $(GNUPLOT_TARFILE)

$(GNUPLOT_TARFILE):
	$(curl_dl) https://zenlayer.dl.sourceforge.net/project/gnuplot/gnuplot/$(GNUPLOT_VERSION)/$(GNUPLOT_TARFILE) -o $(GNUPLOT_TARFILE)
