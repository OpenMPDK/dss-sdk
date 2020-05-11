#! /usr/bin/bash
set -e

UFM_VER="${1:-0}"
UFM_BROKER_VER="${2:-0}"

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/../

pushd "${top_dir}/ufm/fabricmanager"
./makeufmpackage.sh -prJ "${UFM_VER}"
cp ./*.rpm "${top_dir}"/ansible/release/
cp ./*.deb "${top_dir}"/ansible/release/
popd

pushd "${top_dir}/ufm/ufm_msg_broker"
./makebrokerpackage.sh -prJ "${UFM_BROKER_VER}"
cp ./*.rpm "${top_dir}"/ansible/release/
cp ./*.deb "${top_dir}"/ansible/release/
popd
