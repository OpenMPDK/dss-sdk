#!/usr/bin/env bash
#
#
# Note: This script is not used by Jenkins, to is
#       nice script to have when building everything
#       from command line
set -e

script_dir=$(readlink -f "$(dirname "$0")")

pushd "${script_dir}"
    ./build_target.sh
    ./build_nkv_agent.sh

    ./build_ufm.sh
popd
