#! /usr/bin/bash
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/..

# Read build environment vars
pushd "${script_dir}"
# shellcheck disable=SC1091
. ./build_env

# Clean existing RPM/DEB packages
echo "Removing existing RPM/DEB packages"
rm -f "${top_dir}"/ansible/release/*.deb
rm -f "${top_dir}"/ansible/release/*.rpm

# Build Fabric Manager
echo "Building Fabric Mananger"
pushd "${top_dir}/ufm/fabricmanager"
./makeufmpackage.sh -prJ "$UFM_VER"
cp ./*.rpm "${top_dir}"/ansible/release/
cp ./*.deb "${top_dir}"/ansible/release/
popd

# Build Message Broker
echo "Building Message Broker"
pushd "${top_dir}/ufm/ufm_msg_broker"
./makebrokerpackage.sh -prJ "$UFM_BROKER_VER"
cp ./*.rpm "${top_dir}"/ansible/release/
cp ./*.deb "${top_dir}"/ansible/release/
popd
