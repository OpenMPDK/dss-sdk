#!/usr/bin/env bash
#
# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e

CWD=$(readlink -f "$(dirname "$0")")
CWDNAME=$(basename "$CWD")
OUTDIR="${CWD}/../${CWDNAME}_out"
PATCH_PATH="${CWD}/openmpdk.patch"

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
}

if [ $# -eq 0 ]
then
  echo "No arguments supplied, please specify arguments as below."
  Help
  exit 128
fi

if [ "$1" == 'kdd' ] || [ "$1" == 'emul' ] || [ "$1" == 'kdd-samsung' ] || [ "$1" == 'kdd-samsung-remote' ]
then
  echo "Building NKV with : $1"
else
  echo "Build argument should be either 'kdd', 'emul' or 'kdd-samsung' or 'kdd-samsung-remote'"
  Help
  exit 128
fi

#Generate openmpdk patch, ** SAMSUNG internal **  use only.
if [ "$2" == "-p" ]
then
  OPENMPDK_URL=$3
  OPENMPDK_BRANCH=$4
  REMOTE_NAME="remote_patch"
  BRANCH_NAME="openmpdk_patch"
  if [ "${OPENMPDK_URL}" != '' ]
  then
    pushd "${CWD}/src/openmpdk"
      echo "Generating openmpdk patch from ${OPENMPDK_URL} and ${OPENMPDK_BRANCH}" 
      git remote add "${REMOTE_NAME}" "${OPENMPDK_URL}"
      echo "Fetching ${REMOTE_NAME}"
      git fetch "${REMOTE_NAME}"
      COMMIT_ID=$(git reflog | head -1 | cut -d ' ' -f 1)
      git branch "${BRANCH_NAME}" "${COMMIT_ID}"
      git config diff.nodiff.command /bin/true
      echo "*.pdf diff=nodiff" >> .gitattributes
      if [ "${OPENMPDK_BRANCH}" != '' ]; then
        git diff "${BRANCH_NAME}" "${REMOTE_NAME}/${OPENMPDK_BRANCH}" > "${PATCH_PATH}"
      else
        git diff "${BRANCH_NAME}" "${REMOTE_NAME}/master" > "${PATCH_PATH}"
      fi
      git remote remove "${REMOTE_NAME}"
      git branch -d "${BRANCH_NAME}"
    popd
  else
    echo "Please specify internal openmpdk url to generate openmpdk patch"
    echo "./build.sh <Build Mode> -p <internal openmpdk url>"
  fi
fi

#Apply openmpdk patch
if [ -f "${PATCH_PATH}" ]
then
  PATCHED_MARKER="${CWD}/src/openmpdk/.patched"
  if [ ! -f "${PATCHED_MARKER}" ]
  then
    pushd "${CWD}/src/openmpdk"
      echo "Applying openmpdk_patch from ${PATCH_PATH}"
      patch -p1 -N < "${PATCH_PATH}"
      touch "${PATCHED_MARKER}"
    popd
  else
    echo "openmpdk patch is already available, skipping this step! "
  fi
else
  echo "openmpdk patch doesn't exist! exit build process"
  exit 1
fi

# Build dss-sdk host
rm -rf "$OUTDIR"
mkdir "$OUTDIR"
pushd "${OUTDIR}"
  case "$1" in
    'kdd')
      cmake "$CWD" -DNKV_WITH_KDD=ON
      ;;
    'emul')
      cmake "$CWD" -DNKV_WITH_EMU=ON
      ;;
    'kdd-samsung')
      cmake "$CWD" -DNKV_WITH_KDD=ON -DNKV_WITH_SAMSUNG_API=ON
      ;;
    'kdd-samsung-remote')
      cmake "$CWD" -DNKV_WITH_KDD=ON -DNKV_WITH_SAMSUNG_API=ON -DNKV_WITH_REMOTE=ON
      ;;
    *)
      echo "Build argument should be one of: 'kdd', 'emul', 'kdd-samsung', 'kdd-samsung-remote'"
      exit 128
      ;;
  esac
  make -j4
popd
