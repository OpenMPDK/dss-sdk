#!/bin/bash

set -o xtrace

cd ./dssd
git apply ../dssd-stats.patch 

cd ../rocksdb
git am ../patches/rocksdb/0*

cd ../spdk_tcp
git am ../patches/spdk/0*

cd ./dpdk/
patch -p1 < ../../patches/dpdk/update_config.diff

cd ../..
