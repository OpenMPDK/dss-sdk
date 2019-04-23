#!/bin/bash

set -o xtrace

CWD="$(pwd)"
CWDNAME=`basename "$CWD"`
OD="${CWD}/../${CWDNAME}_out"

if [ $(yum list installed | cut -f1 -d" " | grep --extended '^boost-devel' | wc -l) -eq 1 ]; then
  echo "boost-devel already installed";
else
  echo "yum install boost-devel"
  yum install boost-devel
fi

rm -rf $OD
mkdir $OD

cd ${OD}
cmake $CWD -DNKV_WITH_KDD=ON
make -j4

exit
