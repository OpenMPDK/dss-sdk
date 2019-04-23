#!/bin/bash

set -o xtrace

CWD="$(pwd)"
CWDNAME=`basename "$CWD"`
OD="${CWD}../${CWDNAME}_out"

if [ $(yum list installed | cut -f1 -d" " | grep --extended '^boost-devel' | wc -l) -eq 1 ]; then
  echo "boost-devel already installed";
else
  echo "yum install boost-devel"
  yum install bosst-devel
fi

rm -rf $OD
mkdir $OD

cd src/logger/log4cpp/
./configure

cd ${OD}
cmake $CWD -DNKV_WITH_KDD=ON
make -j4

exit
