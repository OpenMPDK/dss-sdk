#!/usr/bin/env bash
#
#
# Note: This script is not used by Jenkins, to is
#       nice script to have when building everything
#       from command line
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/..
release_dir="${top_dir}/ansible/release"

die()
{
    echo "$*"
    exit 1
}

pushd "${script_dir}"
    [[ -d ${release_dir} ]] || mkdir -p ${release_dir}

    ./build_target.sh
    ./build_nkv_agent_only.sh

    ./build_ufm.sh
popd
