#!/bin/bash

DIST_DIR=$(dirname $0)
EXE=${DIST_DIR}/Shasha

LD_LIBRARY_PATH=$DIST_DIR $EXE $@
