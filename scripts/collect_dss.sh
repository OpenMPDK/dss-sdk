#!/usr/bin/env bash
#
#
Help()
{
  echo
  echo "Usage: ./collect_dss.sh"
  echo
  echo
}

set -e
script_dir=$(readlink -f "$(dirname "$0")")

pushd "${script_dir}"

mkdir -p ../dss_out
find ../ -type f -name "*.rpm" -exec cp {} ../dss_out/ \;
find ../ -type f -name "*.tgz" -exec cp {} ../dss_out/ \;

popd
