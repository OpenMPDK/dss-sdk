#!/usr/bin/env bash
#
#
set -e

script_dir=$(readlink -f "$(dirname "$0")")

pushd "${script_dir}"
    ./build_target_only.sh
    #./build_nkv_agent.sh
popd

