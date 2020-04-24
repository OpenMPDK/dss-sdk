#!/bin/bash
Help()
{
  echo
  echo "Usage: ./build.sh <Build Mode>"
  echo 
  echo "Build Modes: kdd, kdd-samsung, kdd-samsung-remote, emul."
  echo "    kdd                : SNIA compatible openmpdk APIs."
  echo "    kdd-samsung        : Samsung openmpdk APIs."
  echo "    kdd-samsung-remote : Samsung openmpdk APIs with remote KV support."
  echo "    emul               : SNIA compatible openmpdk APIs with emulator support."
  echo
  echo "*** Developer's Only ***"
  echo "Generate openmpdk patch and build host software."
  echo
  echo "./build.sh <Build Mode> -p <Gitlab openmpdk url>"
  echo 
}


if [ $# -eq 0 ]
then
  echo "No arguments supplied, please specify arguments as below."
  Help
  exit
fi

if [ $1 == 'kdd' ] || [ $1 == 'emul' ] || [ $1 == 'kdd-samsung' ] || [ $1 == 'kdd-samsung-remote' ]
then
  echo "Building NKV with : $1"
else
  echo "Build argument should be either 'kdd', 'emul' or 'kdd-samsung' or 'kdd-samsung-remote'"
  Help
  exit
fi

set -o xtrace

CWD="$(pwd)"
CWDNAME=`basename "$CWD"`
OD="${CWD}/../${CWDNAME}_out"

#Generate openmpdk patch
if [ $2 == "-p" ]; then
  openmpdk_url=$3
  if [ ${openmpdk_url} != '' ]; then
    cd "src/openmpdk"
    echo "Generating openmpdk patch from ${openmpdk_url}" 
    git remote add gitlab_one ${openmpdk_url}
    git fetch gitlab_one
    commit_id=`git rev-list --tags --max-count=1`
    git branch openmpdk_patch ${commit_id}
    git diff openmpdk_patch gitlab_one/master > ${CWD}/openmpdk.patch
    git remote remove gitlab_one
    git branch -d openmpdk_patch
    cd ${CWD}
  else
    echo "Please specify gitlab openmpdk url to generate openmpdk patch"
    echo "./build.sh <Build Mode> -p <Gitlab openmpdk url>"
  fi
fi
#Apply openmpdk patch
openmpdk_patch="openmpdk.patch"
if [ -f ${openmpdk_patch} ]; then
  patched="src/openmpdk/.patched"
  if [ ! -f ${patched} ]; then
    cd "src/openmpdk"
    echo "Applying openmpdk_patch from ${openmpdk_patch} "
    openmpdk_patch=../../${openmpdk_patch}
    patch -p1 -N < ${openmpdk_patch}
    touch .patched
    cd ${CWD}
  else
    echo "openmpdk patch is already available, skipping this step! "
  fi
else
  echo "openmpdk patch doesn't exist! exit build process"
  exit
fi

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
elif [ $1 == 'kdd-samsung-remote' ]
then
  cmake $CWD -DNKV_WITH_KDD=ON -DNKV_WITH_SAMSUNG_API=ON -DNKV_WITH_REMOTE=ON
else
  echo "Build argument should be either 'kdd' or 'emul' or 'kdd-samsung' or 'kdd-samsung-remote'"
  exit
fi
make -j4

exit
