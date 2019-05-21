#!/bin/bash

if [ $# -eq 0 ]
then
  echo "No arguments supplied, argument should be either 'kdd' or 'emul'"
  exit
fi

if [ $1 == 'kdd' ] || [ $1 == 'emul' ]
then
  echo "Building NKV with : $1"
else
  echo "Build argument should be either 'kdd' or 'emul'"
  exit
fi

set -o xtrace

CWD="$(pwd)"
CWDNAME=`basename "$CWD"`
OD="${CWD}/../${CWDNAME}_out"

rm -rf $OD
mkdir $OD

cd ${OD}
if [ $1 == 'kdd' ]
then
  cmake $CWD -DNKV_WITH_KDD=ON
elif [ $1 == 'emul' ]
then
  cmake $CWD -DNKV_WITH_EMU=ON
else
  echo "Build argument should be either 'kdd' or 'emul'"
  exit
fi
make -j4

exit
