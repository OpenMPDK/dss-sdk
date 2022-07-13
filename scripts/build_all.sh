#!/usr/bin/env bash
#
#
# Note: This script is not used by Jenkins, to is
#       nice script to have when building everything
#       from command line
set -e

Help()
{
  echo
  echo "Usage: ./build_all.sh <NKV Build Mode>"
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
  exit
fi

set -e
script_dir=$(readlink -f "$(dirname "$0")")

pushd "${script_dir}"

  if [ "$1" == 'kdd' ] || [ "$1" == 'emul' ] || [ "$1" == 'kdd-samsung' ] || [ "$1" == 'kdd-samsung-remote' ]
  then
    if [ "$2" == "-p" ]; then
      openmpdk_url=$3
      if [ "${openmpdk_url}" != '' ]; then
        echo "###############################"
        echo "### Building DSS Target ###"
        echo "###############################"
        sleep 4
        ./build_target.sh
        sleep 5
       
        echo "###############################"
        echo "### Building DSS Host ###"
        echo "###############################"
        sleep 4
        pushd "${script_dir}/../host"
        ./build.sh "$1" "$2" "${openmpdk_url} "
        popd
        sleep 5
      else

        echo "Please specify gitlab openmpdk url to generate openmpdk patch"
        echo "./build.sh <Build Mode> -p <Gitlab openmpdk url>"
        popd
        exit
      fi

    else
      echo "###############################"
      echo "### Building DSS Target ###"
      echo "###############################"
      sleep 4
      ./build_target.sh
      sleep 5
      echo "###############################"
      echo "### Building DSS Host ###"
      echo "###############################"
      sleep 4
      pushd "${script_dir}/../host"
      ./build.sh "$1"
      popd
      sleep 5
    fi

  else
    Help
    popd
    exit
  fi
 
  echo "###############################"
  echo "### Building UFM ###"
  echo "###############################"
  sleep 4
  ./build_nkv_agent.sh
  ./build_ufm.sh

popd
