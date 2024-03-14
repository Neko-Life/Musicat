#/bin/bash

DN="$(dirname $0)"
rm -rf $DN/libs/*/ $DN/build/* $DN/libs/icu.tgz
rm -rf $DN/libs/gnuplot*
cd $DN
git submodule update --init --recursive
