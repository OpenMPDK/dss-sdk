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

if [ $(yum list installed | cut -f1 -d" " | grep --extended '^boost-devel' | wc -l) -eq 1 ]; then
  echo "boost-devel already installed";
else
  echo "yum install boost-devel"
  yum install boost-devel
fi

if [ $(yum list installed | cut -f1 -d" " | grep --extended '^log4cpp' | wc -l) -eq 1 ]; then
  echo "log4cpp already installed";
else
  echo "yum install log4cpp"
  yum install log4cpp
fi

if [ $(yum list installed | cut -f1 -d" " | grep --extended '^log4cpp-devel' | wc -l) -eq 1 ]; then
  echo "log4cpp-devel already installed";
else
  echo "yum install log4cpp"
  yum install log4cpp-devel
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
else
  echo "Build argument should be either 'kdd' or 'emul'"
  exit
fi
make -j4

exit
