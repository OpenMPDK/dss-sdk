#!/bin/bash

set -o xtrace

CWD="$(pwd)"

if [ "$#" -ne 1 ]; then
    TGT_VER="0.5.0"
else
    TGT_VER="$1"
fi
TGT_HASH=`git rev-parse --verify HEAD`
sed -i -e "s/^\#define OSS_TARGET_VER.\+$/#define OSS_TARGET_VER \"$TGT_VER\"/" include/version.h
sed -i -e "s/^\#define OSS_TARGET_HASH.\+$/#define OSS_TARGET_HASH \"$TGT_HASH\"/" include/version.h

#source ~/.dragonfly

CWDNAME=`basename "$CWD"`
#OD="${CWD}/${CWDNAME}_out"
OD="${CWD}/../df_out"

rm -rf $OD

cd ./oss
./apply-patch.sh

mkdir $OD
cd ${OD}

cmake $CWD -DCMAKE_BUILD_TYPE=Debug
make spdk_tcp
