#!/usr/bin/env bash
set -e

set -o xtrace

pushd dssd
git checkout .
git apply ../dssd-stats.patch 
popd

pushd ..
#git am ../patches/rocksdb/0*
./scripts/apply_patch.sh rocksdb
popd

pushd spdk_tcp/dpdk
git checkout .
rm meson_options.text.rej
popd

pushd ..
#git am ../patches/spdk/0*
./scripts/apply_patch.sh spdk_tcp spdk
popd

pushd spdk_tcp/dpdk
patch -p1 < ../../patches/dpdk/update_config.diff
popd 
