#!/usr/bin/env bash
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/..

# Read build environment vars
pushd "${script_dir}"

# shellcheck disable=SC1091
source ./build_env

# Build Fabric Manager
echo "Building Fabric Mananger"

pushd "${top_dir}/ufm/fabricmanager"
./makeufmpackage.sh -prJ "$UFM_VER"
popd

# Build Message Broker
echo "Building Message Broker"

pushd "${top_dir}/ufm/ufm_msg_broker"
./makebrokerpackage.sh -pr -J "$UFM_BROKER_VER"

popd
