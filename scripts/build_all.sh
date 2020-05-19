#! /usr/bin/bash
set -e

script_dir=$(readlink -f "$(dirname "$0")")

pushd "${script_dir}"
sh ./build_target.sh
sh ./build_ufm.sh
popd
