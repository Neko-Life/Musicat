ICU_TGZ = $(PWD)/icu.tgz
ICUDATA_SO = $(PWD)/icu/usr/local/lib/libicudata.so

CURLPP_SO = $(PWD)/curlpp/build/libcurlpp.so

LIBOGGZ_BUILD_DIR = $(PWD)/liboggz/build/
LIBOGGZ_SO = $(LIBOGGZ_BUILD_DIR)lib/liboggz.so
LIBOGGZ_BUILD_ENVS = CC=gcc LDFLAGS=-flto

LIBOPUSENC_BUILD_DIR = $(PWD)/libopusenc/build/
LIBOPUSENC_SO = $(LIBOPUSENC_BUILD_DIR)lib/libopusenc.so

GNUPLOT_DIR = gnuplot-$(GNUPLOT_VERSION)
GNUPLOT_BUILD_DIR = $(PWD)/$(GNUPLOT_DIR)/build/
GNUPLOT_BIN = $(GNUPLOT_BUILD_DIR)bin/gnuplot
GNUPLOT_CONFIGURE = $(PWD)/$(GNUPLOT_DIR)/configure
GNUPLOT_TARFILE = $(GNUPLOT_DIR).tar.gz
GNUPLOT_DL_TAR = https://zenlayer.dl.sourceforge.net/project/gnuplot/gnuplot/$(GNUPLOT_VERSION)/$(GNUPLOT_TARFILE)
GNUPLOT_BUILD_ENVS =

ifeq ($(COMPILE_GNUPLOT),1)
	override GNUPLOT_TARGET = gnuplot
endif

ALL_DEPS = icu curlpp $(GNUPLOT_TARGET) libopusenc # liboggz 

all: options $(ALL_DEPS)

options: projects-options
	echo "-- INFO: Build options:"
	echo "-- PWD=${PWD}"
	echo "-- CC=${CC}"
	echo "-- CXX=${CXX}"
	echo "-- LDFLAGS=${LDFLAGS}"
	echo "-- CFLAGS=${CFLAGS}"
	echo "-- CXXFLAGS=${CXXFLAGS}"
	echo "-- WITH_LTO=${WITH_LTO}"
	echo "-- COMPILE_GNUPLOT=${WITH_COMPILE_GNUPLOT}"
	echo "-- GNUPLOT_VERSION=${GNUPLOT_VERSION}"
	echo "-- ALL_DEPS=${ALL_DEPS}"
	echo

projects-options:
	echo "-- INFO: Projects options:"
	echo "-- ICU_TGZ=${ICU_TGZ}"
	echo "-- ICUDATA_SO=${ICUDATA_SO}"
	echo "-- CURLPP_SO=${CURLPP_SO}"
	echo "-- LIBOGGZ_BUILD_DIR=${LIBOGGZ_BUILD_DIR}"
	echo "-- LIBOGGZ_SO=${LIBOGGZ_SO}"
	echo "-- LIBOGGZ_BUILD_ENVS=${LIBOGGZ_BUILD_ENVS}"
	echo "-- LIBOPUSENC_BUILD_DIR=${LIBOPUSENC_BUILD_DIR}"
	echo "-- LIBOPUSENC_SO=${LIBOPUSENC_SO}"
	echo "-- GNUPLOT_DIR=${GNUPLOT_DIR}"
	echo "-- GNUPLOT_BUILD_DIR=${GNUPLOT_BUILD_DIR}"
	echo "-- GNUPLOT_BIN=${GNUPLOT_BIN}"
	echo "-- GNUPLOT_CONFIGURE=${GNUPLOT_CONFIGURE}"
	echo "-- GNUPLOT_TARFILE=${GNUPLOT_TARFILE}"
	echo "-- GNUPLOT_DL_TAR=${GNUPLOT_DL_TAR}"
	echo "-- GNUPLOT_BUILD_ENVS=${GNUPLOT_BUILD_ENVS}"
	echo

################################################################################

curl_dl = curl -L
tar_extract = tar --skip-old-files -xf

icu: $(ICUDATA_SO)

$(ICUDATA_SO): $(ICU_TGZ)
	$(tar_extract) icu.tgz

$(ICU_TGZ):
	$(curl_dl) https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-Ubuntu22.04-x64.tgz -o icu.tgz

curlpp: $(CURLPP_SO)

$(CURLPP_SO):
	cd curlpp && mkdir -p build && cd build && cmake .. && make

libopusenc: $(LIBOPUSENC_SO)

$(LIBOPUSENC_SO):
	cd libopusenc && ./autogen.sh && ./configure --prefix=$(LIBOPUSENC_BUILD_DIR) --disable-examples --disable-doc --enable-year2038 --enable-static=no && make install

liboggz: $(LIBOGGZ_SO)

$(LIBOGGZ_SO):
	cd liboggz && $(LIBOGGZ_BUILD_ENVS) ./autogen.sh && $(LIBOGGZ_BUILD_ENVS) ./configure --prefix=$(LIBOGGZ_BUILD_DIR) && $(LIBOGGZ_BUILD_ENVS) make install

gnuplot: $(GNUPLOT_BIN)

$(GNUPLOT_BIN): $(GNUPLOT_CONFIGURE)
	cd $(GNUPLOT_DIR) && $(GNUPLOT_BUILD_ENVS) ./configure --prefix=$(GNUPLOT_BUILD_DIR) --with-texdir=$(GNUPLOT_BUILD_DIR)texmf --with-qt=no --with-x=no && $(GNUPLOT_BUILD_ENVS) make install

$(GNUPLOT_CONFIGURE): $(GNUPLOT_TARFILE)
	$(tar_extract) $(GNUPLOT_TARFILE)

$(GNUPLOT_TARFILE):
	echo 'Downloading '$(GNUPLOT_DL_TAR)
	$(curl_dl) $(GNUPLOT_DL_TAR) -o $(GNUPLOT_TARFILE)
