#!/bin/bash

if [ $# -eq 0 ]
then
  echo "No arguments supplied, argument should be either 'kdd', 'kdd-samsung' or 'emul'"
  exit
fi

if [ $1 == 'kdd' ] || [ $1 == 'emul' ] || [ $1 == 'kdd-samsung' ]
then
  echo "Building NKV with : $1"
else
  echo "Build argument should be either 'kdd', 'emul' or 'kdd-samsung'"
  exit
fi

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

# libcurl installation
if [  $(yum list installed | cut -f1 -d" " | grep --extended '^libcurl-devel' | wc -l) -eq 0 ]; then
  echo "yum install libcurl-devel"
  yum install libcurl-devel
fi

#libnuma installation
if [ $(yum list installed | cut -f1 -d" " | grep --extended '^numactl-devel' | wc -l) -eq 0 ]; then
  echo "yum install numactl-devel"
  yum install numactl-devel
fi

#intel tbb installation , required for OpenMPDK
if [ $(yum list installed | cut -f1 -d" " | grep --extended '^tbb-devel' | wc -l) -eq 0 ]; then
  echo "yum install tbb-devel"
  yum install tbb-devel
fi

rm -rf $OD
mkdir $OD
cd ${OD}
if [ $1 == 'kdd' ]
then
  cmake $CWD -DNKV_WITH_KDD=ON
elif [ $1 == 'emul' ]
then
  cmake $CWD -DNKV_WITH_EMU=ON
elif [ $1 == 'kdd-samsung' ]
then
  cmake $CWD -DNKV_WITH_KDD=ON -DNKV_WITH_SAMSUNG_API=ON
else
  echo "Build argument should be either 'kdd' or 'emul'"
  exit
fi
make -j4

exit
