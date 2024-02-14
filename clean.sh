#/bin/bash

DN="$(dirname $0)"
rm -rf $DN/libs/*/ $DN/build/* $DN/libs/icu.tgz
cd $DN
git submodule update --init --recursive
