#!/usr/bin/env bash
set -e

Help()
{
  echo
  echo "Usage: ./collect_dss.sh"
  echo
  echo
}

script_dir=$(readlink -f "$(dirname "$0")")

pushd "${script_dir}"

mkdir -p "${script_dir}/../dss_out"
rm -rf "${script_dir}/../dss_out/*"
find "${script_dir}/../host_out" -type f -name "*.rpm" -exec cp {} "${script_dir}/../dss_out/" \;
find "${script_dir}/../host_out" -type f -name "*.tgz" -exec cp {} "${script_dir}/../dss_out/" \;

find "${script_dir}/../df_out" -type f -name "*.rpm" -exec cp {} "${script_dir}/../dss_out/" \;
find "${script_dir}/../df_out" -type f -name "*.tgz" -exec cp {} "${script_dir}/../dss_out/" \;

find "${script_dir}/../ufm" -type f -name "*.rpm" -exec cp {} "${script_dir}/../dss_out/" \;
find "${script_dir}/../ufm" -type f -name "*.tgz" -exec cp {} "${script_dir}/../dss_out/" \;

popd
